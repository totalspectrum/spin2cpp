/*
 * PASM and data compilation
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "spinc.h"

#ifndef NEED_ALIGNMENT
#define NEED_ALIGNMENT (!gl_p2 && !gl_compress)
#endif

static char *
NewOrgName()
{
    static int counter;
    char *buf;
    ++counter;
    buf = calloc(1, 32);
    if (!buf) return buf;
    sprintf(buf, "..org%04x", counter);
    return buf;
}

unsigned
InstrSize(AST *instr)
{
    AST *ast;
    unsigned size = 4;
    while(instr) {
        ast = instr;
        instr = instr->right;
        while (ast && ast->kind == AST_EXPRLIST) {
            ast = ast->left;
        }
        if (ast && ast->kind == AST_BIGIMMHOLDER) {
            size += 4;
        }
    }
    return size;
}

/*
 * find number of bytes required for an FVAR item
 */
unsigned
BytesForFvar(AST *item, int isSigned)
{
    int32_t val;

    if (!item || item->kind != AST_EXPRLIST) {
        return 0;
    }
    
    val = EvalPasmExpr(item->left);

    if (isSigned) {
        if (val >= -64 && val < 64) return 1;
        if (val >= -(1<<13) && val < (1<<13)) return 2;
        if (val >= -(1<<20) && val < (1<<20)) return 3;
        if (val >= -(1<<28) && val < (1<<28)) return 4;
        ERROR(item, "FVARS value ($%lx) out of range", val);
    } else {
        if (val < 0) {
            ERROR(item, "FVARS value ($%lx) out of range", val);
        }
        if (val < (1<<7)) return 1;
        if (val < (1<<14)) return 2;
        if (val < (1<<21)) return 3;
        if (val < (1<<29)) return 4;
    }
    return 4;
}

/*
 * find the length of a data list, in bytes
 */
unsigned
dataListLen(AST *ast, int elemsize)
{
    unsigned size = 0;
    unsigned numelems;
    int origelemsize = elemsize;
    AST *sub;

    while (ast) {
        sub = ast->left;
        if (sub) {
            if (sub->kind == AST_LONGLIST) {
                elemsize = 4;
                sub = ExpectOneListElem(sub->left);
            } else if (sub->kind == AST_WORDLIST) {
                elemsize = 2;
                sub = ExpectOneListElem(sub->left);
            } else if (sub->kind == AST_BYTELIST) {
                elemsize = 1;
                sub = ExpectOneListElem(sub->left);
            } else {
                elemsize = origelemsize;
            }
            if (sub->kind == AST_ARRAYDECL || sub->kind == AST_ARRAYREF) {
                numelems = EvalPasmExpr(ast->left->right);
                if ((int)numelems < 0) {
                    ERROR(sub, "Negative repeat count not allowed");
                    numelems = 0;
                }
            } else if (sub->kind == AST_STRING) {
                numelems = strlen(sub->d.string);
            } else if (sub->kind == AST_RANGE) {
                int start = EvalPasmExpr(sub->left);
                numelems = (EvalPasmExpr(sub->right) - start) + 1;
                if ((int)numelems < 0) {
                    ERROR(sub, "Backwards range not supported");
                    numelems = 0;
                }
            } else if (sub->kind == AST_FVAR_LIST) {
                elemsize = 1;
                numelems = BytesForFvar(sub->left, 0);
            } else if (sub->kind == AST_FVARS_LIST) {
                elemsize = 1;
                numelems = BytesForFvar(sub->left, 1);
            } else {
                numelems = 1;
            }
            size += (numelems*elemsize);
        }
        ast = ast->right;
    }
    return size;
}

/*
 * align a pc on a boundary
 */
unsigned
align(unsigned pc, int size)
{
    pc = (pc + (size-1)) & ~(size-1);
    return pc;
}

/*
 * enter a label
 */
void
EnterLabel(Module *P, AST *origLabel, long hubpc, long cogpc, AST *ltype, Symbol *lastorg, int inHub, unsigned flags)
{
    const char *name;
    Label *labelref;
    Symbol *sym;

    name = origLabel->d.string;
    sym = FindSymbol(&P->objsyms, name);
    if (sym && sym->kind == SYM_WEAK_ALIAS) {
        // it's OK to redefine a weak alias
        sym = NULL;
    }
    if (sym) {
        // redefining a label with the exact same values is OK
        if (sym->kind != SYM_LABEL) {
            AST *olddef = (AST *)sym->def;
            ERROR(origLabel, "Redefining symbol %s", name);
            if (olddef) {
                ERROR(olddef, "...previous definition was here");
            }
            return;
        }
        labelref = (Label *)sym->val;
#if 1
        if (labelref->hubval != hubpc) {
            ERROR(origLabel, "Changing hub value for symbol %s", name);
            return;
        }
        if (labelref->cogval != cogpc) {
            ERROR(origLabel, "Changing cog value for symbol %s", name);
            return;
        }
#ifdef NEVER
        // labelref->org is never actually used, so do not bother checking it
        // also, a proper check would actually involve the value rather than
        // just the pointer
        if (labelref->org != lastorg) {
            ERROR(origLabel, "Changing lastorg value for symbol %s", name);
            return;
        }
#endif        
        if (!CompatibleTypes(labelref->type, ltype)) {
            ERROR(origLabel, "Changing type of symbol %s", name);
            return;
        }
        if (ltype && labelref->size != TypeSize(ltype)) {
            ERROR(origLabel, "Changing size of symbol %s from %d to %d", name,
                  labelref->size, TypeSize(ltype));
            return;
        }            
        if (inHub) {
            if (!(labelref->flags & LABEL_IN_HUB)) {
                ERROR(origLabel, "Changing inhub value of symbol %s", name);
                return;
            }
        } else if (labelref->flags & LABEL_IN_HUB) {
            ERROR(origLabel, "Changing inhub value of symbol %s", name);
            return;
        }
        return;
#endif        
    } else {
        labelref = (Label *)calloc(1, sizeof(*labelref));
    }
    if (!labelref) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    labelref->hubval = hubpc;
    labelref->cogval = cogpc;
    labelref->type = ltype;
    labelref->org = lastorg;
    labelref->size = ltype ? TypeSize(ltype) : 4;
    if (inHub) {
        flags |= LABEL_IN_HUB;
    }
    labelref->flags = flags;

    if (!sym) {
        sym=AddSymbolPlaced(&P->objsyms, name, SYM_LABEL, labelref, NULL, origLabel);
    }
}

/*
 * emit pending labels
 */
AST *
emitPendingLabels(Module *P, AST *label, unsigned hubpc, unsigned cogpc, AST *ltype, Symbol *lastorg, int inHub, unsigned flags)
{
    while (label) {
        EnterLabel(P, label->left, hubpc, cogpc, ltype, lastorg, inHub, flags);
        //printf("label: %s = %x\n", label->left->d.string, hubpc);
        label = label->right;
    }
    return NULL;
}

/*
 * replace AST_HERE with AST_INTEGER having pc as its value, if necessary
 * otherwise update the AST_HERE with the value of the last origin symbol
 */
static void
replaceHeres(AST *ast, uint32_t newpc, Symbol *lastOrg)
{
    if (ast == NULL)
        return;
    if (ast->kind == AST_HERE) {
        if (gl_gas_dat) {
            ast->d.ptr = (void *)lastOrg;
        } else {
            ast->kind = AST_INTEGER;
            ast->d.ival = newpc;
        }
        return;
    }
    replaceHeres(ast->left, newpc, lastOrg);
    replaceHeres(ast->right, newpc, lastOrg);
}

/*
 * do replaceHeres on each element of a long list
 */
static void
replaceHereDataList(AST *ast, int inHub, uint32_t newpc, uint32_t inc, Symbol *lastOrg)
{
    AST *sub;

    while (ast) {
        sub = ast->left;
        if (sub) {
            replaceHeres(sub, inHub ? newpc : newpc / 4, lastOrg);
        }
        newpc += inc;
        ast = ast->right;
    }
}

/*
 * find the length of a file
 */
static long
filelen(AST *ast)
{
    FILE *f;
    const char *name = ast->d.string;
    int r;
    long siz;

    f = fopen(name, "rb");
    if (!f) {
        ERROR(ast, "file %s: %s", name, strerror(errno));
        return 0;
    }
    r = fseek(f, 0L, SEEK_END);
    if (r < 0) {
        ERROR(ast, "file %s: %s", name, strerror(errno));
        return 0;
    }
    siz = ftell(f);
    fclose(f);
    return siz;
}

static AST *
reduceStrings(AST *orig_exprlist)
{
    AST *exprlist = orig_exprlist;
    AST *elem, *next;
    AST *first = NULL;
    ASTReportInfo saveinfo;
    
    if (!exprlist || exprlist->kind != AST_EXPRLIST) {
        ERROR(exprlist, "internal error, expected exprlist");
        return exprlist;
    }
    while (exprlist) {
        elem = exprlist->left;
        next = exprlist->right;
        if (elem->kind == AST_STRING) {
            AST *ast = NULL;
            const char *t = elem->d.string;
            int c;
            first = elem;
            AstReportAs(elem, &saveinfo);
            do {
                c = *t++;
                if (!c) break;
                ast = AstInteger(c);
                if (first) {
                    *first = *ast;
                    first = NULL;
                } else {
                    ast = NewAST(AST_EXPRLIST, ast, NULL);
                    exprlist->right = ast;
                    ast->right = next;
                    exprlist = ast;
                }
            } while (c);
            AstReportDone(&saveinfo);
        }
        exprlist = next;
    }
    // add a trailing 0
    exprlist = AddToList(orig_exprlist, NewAST(AST_EXPRLIST, AstInteger(0), NULL));
    return exprlist;
}

static AST *
PullFromList(int n, AST *list, AST **leftover)
{
    AST *first;
    
    if (n > 0) {
        ASTReportInfo saveinfo;
        AstReportAs(list, &saveinfo);
        first = NewAST(AST_EXPRLIST, list->left, NULL);
        AstReportDone(&saveinfo);
        first->right = list->right;
        --n;
    } else {
        first = NULL;
    }
    while (n > 0 && list->right) {
        list = list->right;
        --n;
    }
    if (leftover) {
        *leftover = list->right;
    }
    list->right = NULL;
    return first;
}

static void
fixupInitializer(Module *P, AST *initializer, AST *type)
{
    AST *newinit;
    AST *newident;
    AST *subtype;
    AST *elem;
    AST *initval = initializer;
    AST *thiselem = 0;
    
    type = RemoveTypeModifiers(type);
    if (!type) {
        return;
    }
    while (initval && initval->kind == AST_CAST) {
        initval = initval->right;
    }
    if (!initval) {
        return;
    }
    if (initval->kind == AST_STRINGPTR) {
        *initializer = *reduceStrings(initval->left);
    }
    if (initval->kind == AST_SIMPLEFUNCPTR) {
        return;
    }
    if (type->kind == AST_PTRTYPE || IsIntType(type)) {
        if (initval->kind == AST_STRINGPTR) {
            elem = initval->left;
        } else {
            AST *typ = ExprType(initval);
            if (IsFunctionType(typ)) {
                ASTReportInfo saveinfo;
                AstReportAs(initval, &saveinfo);
                elem = initval;
                if (IsIdentifier(initval)) {
                    *initval = *NewAST(AST_ABSADDROF, DupAST(initval), NULL);
                }
                while (elem && (elem->kind == AST_ADDROF || elem->kind == AST_ABSADDROF)) {
                    elem = elem->left;
                }
                elem = NewAST(AST_SIMPLEFUNCPTR, elem, NULL);
                elem = NewAST(AST_EXPRLIST,
                              AstInteger(0),
                              NewAST(AST_EXPRLIST, elem, NULL));
                type = ast_type_ptr_long;
                AstReportDone(&saveinfo);
            } else {
                elem = initval;
            }
        }
        if (elem->kind == AST_EXPRLIST) {
            AST *ast, *declare;
            AST *arrayref;
            ASTReportInfo saveinfo;

            AstReportAs(elem, &saveinfo);
            /* need to move it to its own declaration */
            if (elem->kind == AST_EXPRLIST) {
                subtype = ExprType(type->left);
                subtype = NewAST(AST_ARRAYTYPE, subtype, AstInteger(AstListLen(elem)));
            } else {
                subtype = ast_type_ptr_void;
            }
            newinit = NewAST(AST_EXPRLIST, NULL, NULL);
            *newinit = *elem;
            newident = AstTempIdentifier("_array_");
            declare = AstAssign(newident, newinit);

            declare = NewAST(AST_DECLARE_VAR, subtype, declare);
            ast = NewAST(AST_COMMENTEDNODE, declare, NULL);
            P->datblock = AddToList(P->datblock, ast);
            arrayref = NewAST(AST_ARRAYREF, newident, AstInteger(0));
            arrayref = NewAST(AST_ABSADDROF, arrayref, NULL);
            arrayref = NewAST(AST_CAST, ast_type_generic, arrayref);
            *initializer = *arrayref;
            AstReportDone(&saveinfo);
        }
    } else  if (type->kind == AST_ARRAYTYPE) {
        AST *elem;
        subtype = type->left;
        if (initializer->kind != AST_EXPRLIST) {
            /* could be a raw list */
            while (IsArrayType(subtype)) {
                subtype = BaseType(subtype);
            }
        }
        for (elem = initializer; elem; elem = elem->right) {
            fixupInitializer(P, elem->left, subtype);
        }
    } else if (type->kind == AST_OBJECT) {
        Module *Q = GetClassPtr(type);
        AST *varlist, *elem;
        AST *thisval = NULL;
        varlist = Q->finalvarblock;
        if (Q->pendingvarblock) {
            ERROR(initializer, "internal error, got something in pendingvarblock");
            return;
        }
        if (initializer->kind != AST_EXPRLIST) {
            ERROR(initializer, "wrong kind of initializer for struct type");
            return;
        }
        for (elem = initializer; elem; elem = elem->right) {
            int n;
            if (!varlist) {
                ERROR(initializer, "too many initializers for struct or union");
                return;
            }
            thiselem = 0;
            while (varlist && varlist->left && varlist->left->kind == AST_DECLARE_BITFIELD) {
                AST *baseval;
                AST *casttype;
                baseval = varlist->left->left;
                if (baseval->kind != AST_CAST) {
                    ERROR(varlist, "internal error 0");
                    return;
                }
                casttype = baseval->left;
                baseval = baseval->right;
                if (baseval->kind != AST_RANGEREF) {
                    ERROR(varlist, "internal error 1");
                    return;
                }
                baseval = baseval->right;
                if (baseval->kind != AST_RANGE) {
                    ERROR(varlist, "internal error 2");
                    return;
                }
                baseval = elem ? AstOperator(K_SHL, elem->left, baseval->right) : AstInteger(0);
                if (thisval) {
                    thisval->right = AstOperator('|', thisval->right, baseval);
                    if (elem) {
                        thiselem->right = elem->right;  /* remove "elem" from list */
                    }
                } else {
                    thisval = NewAST(AST_CAST, casttype, baseval);
                    thiselem = elem;
                    elem->left = thisval;
                }
                varlist = varlist->right;
                if (elem) {
                    elem = elem->right;
                }
            }
            if (!thisval) {
                thisval = elem ? elem->left : AstInteger(0);
            }
            if (thiselem) {
                elem = thiselem;
            }
            subtype = ExprType(varlist->left);
            varlist = varlist->right;
            if (Q->isUnion) varlist = NULL;
            if ( (IsArrayType(subtype) || IsClassType(subtype)) && thisval && (thisval->kind != AST_EXPRLIST && thisval->kind != AST_STRINGPTR)) {
                AST *newsub;
                AST *oldlist;
                n = AggregateCount(subtype);
                newsub = PullFromList(n, elem, &oldlist);
                elem->left = thisval = newsub;
                elem->right = oldlist;
            }
            fixupInitializer(P, thisval, subtype);
            thisval = 0;
        }
    }
}

/*
 * declare labels for a data block
 */

#define ALIGNPC(size)  do { inc = size; cogpc = align(cogpc, inc); datoff = align(datoff, inc); hubpc = align(hubpc, inc); } while (0)
#define MAYBEALIGNPC(size) if (NEED_ALIGNMENT) { ALIGNPC(size); }
#define INCPC(size)  do { inc = size; cogpc += inc; datoff += inc; hubpc += inc; } while (0)
#define HEREPC (inHub ? hubpc : (cogpc/4))

#define MARK_DATA(label_flags) label_flags &= ~LABEL_HAS_INSTR
#define MARK_CODE(label_flags) label_flags |= LABEL_HAS_INSTR
#define MARK_HUB(label_flags) label_flags |= LABEL_IN_HUB
#define MARK_COG(label_flags) label_flags &= ~LABEL_IN_HUB

static bool
IsJmpRetInstruction(AST *ast)
{
    while (ast && (ast->kind == AST_COMMENT || ast->kind == AST_SRCCOMMENT)) {
        ast = ast->right;
    }
    if (!ast) return false;
    if (ast->kind == AST_COMPRESS_INSTR) {
        ast = ast->left;
    }
    if (ast && ast->kind == AST_INSTR) {
        Instruction *instr = (Instruction *)ast->d.ptr;
        if (instr && (instr->opc == OPC_RET || instr->opc == OPC_JMPRET) && !gl_p2) {
            return true;
        }
    }
    return false;
}

void
DeclareLabels(Module *P)
{
    unsigned cogpc = 0;
    unsigned hubpc = 0;
    unsigned datoff = 0;
    unsigned inc = 0;
    unsigned delta;
    int inHub = 0;
    
    AST *top = NULL;
    AST *ast = NULL;
    AST *pendingLabels = NULL;
    AST *lasttype = ast_type_long;
    Symbol *lastOrg = NULL;
    const char *tmpName;
    unsigned label_flags = 0;
    
    P->gasPasm = gl_gas_dat;
    if (gl_no_coginit && gl_output == OUTPUT_DAT) {
        // insert an implicit orgh P2_HUB_BASE
        hubpc = gl_hub_base;
        inHub = 1;
        MARK_HUB(label_flags);
    }
    for (top = P->datblock; top; top = top->right) {
        ast = top;
        while (ast && ast->kind == AST_COMMENTEDNODE) {
            ast = ast->left;
        }
        if (!ast) continue;
        switch (ast->kind) {
        case AST_BYTELIST:
            MARK_DATA(label_flags);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_byte, lastOrg, inHub, label_flags);
            replaceHereDataList(ast->left, inHub, inHub ? hubpc : cogpc, 1, lastOrg);
            INCPC(dataListLen(ast->left, 1));
            lasttype = ast_type_byte;
            break;
        case AST_WORDLIST:
            MARK_DATA(label_flags);
            MAYBEALIGNPC(2);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_word, lastOrg, inHub, label_flags);
            replaceHereDataList(ast->left, inHub, inHub ? hubpc : cogpc, 2, lastOrg);
            INCPC(dataListLen(ast->left, 2));
            lasttype = ast_type_word;
            break;
        case AST_LONGLIST:
            MARK_DATA(label_flags);
            MAYBEALIGNPC(4);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub, label_flags);
            replaceHereDataList(ast->left, inHub, inHub ? hubpc : cogpc, 4, lastOrg);
            INCPC(dataListLen(ast->left, 4));
            lasttype = ast_type_long;
            break;
        case AST_INSTRHOLDER:
            MARK_CODE(label_flags);
            if (inHub) {
                MAYBEALIGNPC(4);
            } else {
                ALIGNPC(4);
            }
            /* check to see if the following instruction is a "ret" */
            if (!gl_p2 && IsJmpRetInstruction(ast->left)) {
                pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub, label_flags | LABEL_HAS_JMP);
            } else {
                pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub, label_flags);
            }
            replaceHeres(ast->left, HEREPC, lastOrg);
            ast->d.ival = inHub ? hubpc : (cogpc | (1<<30));
            INCPC(InstrSize(ast->left));
            lasttype = ast_type_long;
            current->datHasCode = 1;
            break;
        case AST_IDENTIFIER:
            pendingLabels = AddToList(pendingLabels, NewAST(AST_LISTHOLDER, ast, NULL));
            break;
        case AST_ORG:
            MAYBEALIGNPC(4);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub, label_flags);
            if (ast->left) {
                replaceHeres(ast->left, HEREPC, lastOrg);
                cogpc = 4*EvalPasmExpr(ast->left);
            } else {
                cogpc = 0;
            }
            tmpName = NewOrgName();
            lastOrg = AddSymbol(&current->objsyms, tmpName, SYM_CONSTANT, AstInteger(cogpc), NULL);
            lasttype = ast_type_long;
            ast->d.ptr = (void *)lastOrg;
            inHub = 0;
            MARK_COG(label_flags);
            break;
        case AST_ORGH:
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub, label_flags);
            ast->d.ival = hubpc; // temporary
            if (ast->left) {
                replaceHeres(ast->left, hubpc, lastOrg);
                hubpc = EvalPasmExpr(ast->left);
            }
            cogpc = hubpc;
            // calculate padding amount
            if (ast->d.ival <= hubpc) {
                ast->d.ival = hubpc - ast->d.ival;
            } else {
                ERROR(ast, "orgh address %x is less than previous address %x",
                      hubpc, ast->d.ival);
                ast->d.ival = 0;
            }
            lasttype = ast_type_long;
            inHub = 1;
            MARK_HUB(label_flags);
            break;
        case AST_ORGF:
            if (inHub) {
                ERROR(ast, "ORGF only valid in cog mode");
                ast->d.ival = 0;
            } else {
                uint32_t newpc = 4*EvalPasmExpr(ast->left);
                uint32_t pad = 0;
                while (cogpc < newpc) {
                    cogpc++;
                    hubpc++;
                    pad++;
                }
                ast->d.ival = pad;
            }
            break;
        case AST_RES:
            MARK_DATA(label_flags);
            if (inHub) {
                ERROR(ast, "res not valid after orgh");
            }
            cogpc = align(cogpc, 4);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub, label_flags);
            replaceHeres(ast->left, cogpc / 4, lastOrg);
            delta = EvalPasmExpr(ast->left);
            if ( ((int)delta) < 0) {
                ERROR(ast, "RES with negative storage specifier");
                delta = 0;
            } else if (delta == 0) {
                WARNING(ast, "RES has 0 space");
            }
            cogpc += 4*delta;
//            hubpc += 4*delta;
            lasttype = ast_type_long;
            break;
        case AST_FIT:
            cogpc = align(cogpc, 4);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub, label_flags);
            if (ast->left) {
                int32_t max = EvalPasmExpr(ast->left);
                int32_t cur = (cogpc) / 4;
                if ( cur > max ) {
                    ERROR(ast, "fit %d failed: pc is %d", max, cur);
                }
            }
            lasttype = ast_type_long;
            break;
        case AST_FILE:
            MARK_DATA(label_flags);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_byte, lastOrg, inHub, label_flags);
            INCPC(filelen(ast->left));
            break;
        case AST_LINEBREAK:
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, lasttype, lastOrg, inHub, label_flags);
            break;
        case AST_COMMENT:
        case AST_SRCCOMMENT:
            break;
        case AST_ALIGN:
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, lasttype, lastOrg, inHub, label_flags);
            ALIGNPC(EvalPasmExpr(ast->left));
            break;
        case AST_DECLARE_VAR:
            {
                AST *type = ast->left;
                AST *ident = ast->right;
                AST *initializer = NULL;
                ASTReportInfo saveinfo;
                int typalign;
                int typsize;

                // initializers always go in HUB memory
                inHub = 1;
                MARK_DATA(label_flags);
                AstReportAs(ident, &saveinfo);
                if (ident->kind == AST_ASSIGN) {
                    initializer = ident->right;
                    ident = ident->left;
                }
                while (ident && ident->kind == AST_ARRAYDECL) {
                    type = MakeArrayType(type, ident->right);
                    ident = ident->left;
                }
                typalign = TypeAlign(type);
                typsize = TypeSize(type);
                if (typsize == 0) {
                    // empty object; this is OK if it has methods
                    Module *Q = GetClassPtr(type);
                    if (Q && !Q->functions) {
                        ERROR(ast, "empty or undefined class used to define %s", GetUserIdentifierName(ident));
                    }
                }
                
                ALIGNPC(typalign);
                if (ident->kind == AST_LOCAL_IDENTIFIER) {
                    ident = ident->left;
                }
                if (ident->kind != AST_IDENTIFIER) {
                    ERROR(ast, "Internal error in DECLARE_VAR: expected identifier");
                } else {
                    pendingLabels = AddToList(pendingLabels, NewAST(AST_LISTHOLDER, ident, NULL));
                }
                pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, type, lastOrg, inHub, label_flags);
                INCPC(typsize);
                fixupInitializer(P, initializer, type);
                AstReportDone(&saveinfo);
            }
            break;
        default:
            ERROR(ast, "unknown element %d in data block", ast->kind);
            break;
        }
    }

    // emit any labels that come at the end
    pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, lasttype, lastOrg, inHub, label_flags);
    
    P->datsize = datoff;
}
