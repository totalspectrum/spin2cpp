/*
 * PASM and data compilation
 * Copyright 2011-2023 Total Spectrum Software Inc.
 * 
 * +--------------------------------------------------------------------
 * ¦  TERMS OF USE: MIT License
 * +--------------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * +--------------------------------------------------------------------
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
    buf = (char *)calloc(1, 32);
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

static int expect_undefined_labels = 0;
static int found_undefined_labels = 0;
static int labels_changed = 0;

unsigned
BytesForFvar(int val, int isSigned, AST *line)
{
    if (isSigned) {
        if (val >= -(1<<6)  && val < (1<<6))  return 1;
        if (val >= -(1<<13) && val < (1<<13)) return 2;
        if (val >= -(1<<20) && val < (1<<20)) return 3;
        if (val >= -(1<<28) && val < (1<<28)) return 4;
        ERROR(line, "FVARS value ($%lx) out of range", (unsigned long)val);
    } else {
        if (val < 0) {
            ERROR(line, "FVARS value ($%lx) out of range", (unsigned long)val);
        }
        if (val < (1<<7)) return 1;
        if (val < (1<<14)) return 2;
        if (val < (1<<21)) return 3;
        if (val < (1<<29)) return 4;
        ERROR(line, "FVARS value ($%lx) out of range", (unsigned long)val);
    }
    return 4;
}

/*
 * find number of bytes required for an FVAR item
 * this is slightly tricky because it can change after
 * labels are assigned
 */
unsigned
BytesForFvarAst(AST *item, int isSigned)
{
    int32_t val;
    
    if (!item || item->kind != AST_EXPRLIST) {
        return 0;
    }

    if (expect_undefined_labels && !IsDefinedExpr(item->left)) {
        found_undefined_labels++;
        return 4;
    } else {
        val = EvalPasmExpr(item->left);
    }
    return BytesForFvar(val, isSigned, item);
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
                numelems = BytesForFvarAst(sub->left, 0);
            } else if (sub->kind == AST_FVARS_LIST) {
                elemsize = 1;
                numelems = BytesForFvarAst(sub->left, 1);
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
 * we may need to make multiple passes, 
 */
void
EnterLabel(SymbolTable *symtab, AST *origLabel, long hubpc, long cogpc, AST *ltype, Symbol *lastorg, int inHub, unsigned flags, unsigned pass)
{
    const char *name;
    Label *labelref;
    Symbol *sym;

    name = origLabel->d.string;
    sym = FindSymbol(symtab, name);
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
        labelref = (Label *)sym->v.ptr;

        if (labelref->hubval != hubpc) {
            if (labelref->pass == pass) {
                ERROR(origLabel, "Changing hub value for symbol %s", name);
                return;
            }
            labels_changed++;
        }
        if (labelref->cogval != cogpc && labelref->pass == pass) {
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
    labelref->pass = pass;
    if (inHub) {
        flags |= LABEL_IN_HUB;
    }
    labelref->flags = flags;

    if (!sym) {
        sym=AddSymbolPlaced(symtab, name, SYM_LABEL, labelref, NULL, origLabel);
        sym->module = current;
    }
}

/*
 * emit pending labels
 */
AST *
emitPendingLabels(SymbolTable *symtab, AST *label, unsigned hubpc, unsigned cogpc, AST *ltype, Symbol *lastorg, int inHub, unsigned flags, unsigned pass)
{
    while (label) {
        EnterLabel(symtab, label->left, hubpc, cogpc, ltype, lastorg, inHub, flags, pass);
        //printf("label: %s = %x\n", label->left->d.string, hubpc);
        label = label->right;
    }
    return NULL;
}

/*
 * replace AST_HERE with AST_HERE_IMM having pc as its value, if necessary
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
            ast->kind = AST_HERE_IMM;
            ast->d.ival = newpc;
        }
        return;
    } else if (ast->kind == AST_HERE_IMM) {
        ast->d.ival = newpc;
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
        ERROR(exprlist, "Internal error, expected exprlist");
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
        if (n > 1) {
            first->right = list->right;
        }
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
    int expectedItems = 0;
    int foundItems = 0;
    
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
                if (ComplexMethodPtrs()) {
                    elem = NewAST(AST_EXPRLIST,
                                  AstInteger(0),
                                  NewAST(AST_EXPRLIST, elem, NULL));
                    type = ast_type_ptr_long;
                } else {
                    *initval = *elem;
                    type = ast_type_ptr_long;
                }
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
            fixupInitializer(current, elem->left, subtype);
        }
    } else if (type->kind == AST_OBJECT) {
        Module *Q = GetClassPtr(type);
        AST *varlist, *elem;
        AST *thisval = NULL;
        varlist = Q->finalvarblock;
        if (Q->pendingvarblock) {
            ERROR(initializer, "Internal error, got something in pendingvarblock");
            return;
        }
        if (initializer->kind != AST_EXPRLIST) {
            if (initializer->kind == AST_INTEGER && initializer->d.ival == 0) {
                /* do nothing */
            } else {
                ERROR(initializer, "wrong kind of initializer for struct type");
            }
            return;
        }
        // calculate how many items to expected
        if (Q->isUnion) {
            expectedItems = 1;
        } else {
            for (elem = varlist; elem; elem = elem->right) {
                expectedItems++;
            }
        }
        for (elem = initializer; elem; elem = elem->right) {
            int n;
            if (!varlist) {
                while (elem) {
                    foundItems++;
                    elem = elem->right;
                }
                ERROR(initializer, "too many initializers for struct or union: expected %d found %d", expectedItems, foundItems);
                return;
            }
            foundItems++;
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
            while (thisval && thisval->kind == AST_CAST) {
                if (Q->isUnion) {
                    subtype = thisval->left;
                }
                thisval = thisval->right;
            }
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

unsigned
AssignAddresses(SymbolTable *symtab, AST *instrlist, int startFlags)
{
    unsigned cogpc = 0;
    unsigned hubpc = 0;
    unsigned coglimit = 0;
    unsigned hublimit = 0;
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
    unsigned pass = 0;
    unsigned orig_datoff = 0;
    
    expect_undefined_labels = 1;
    unsigned asm_nest;
    AsmState state[MAX_ASM_NEST] = { 0 };
    
again:
    labels_changed = 0;
    
    cogpc = coglimit = hublimit = 0;
    hubpc = 0;
    datoff = 0;
    inc = 0;
    inHub = 0;

    asm_nest = 0;
    state[asm_nest].is_active = true;
    
    pass++;
    if (startFlags == ADDRESS_STARTFLAG_HUB) {
        // insert an implicit orgh P2_HUB_BASE
        hubpc = gl_hub_base;
        inHub = 1;
        MARK_HUB(label_flags);
    }
    for (top = instrlist; top; top = top->right) {
        ast = top;
        while (ast && ast->kind == AST_COMMENTEDNODE) {
            ast = ast->left;
        }
        if (!ast) continue;
        if (inHub) {
            if (hublimit && hubpc >= hublimit) {
                ERROR(ast, "HUB PC of $%lx exceeds limit of $%lx\n", (long)hubpc, (long)hublimit);
                hublimit = 0;
            }
        } else {
            if (coglimit && cogpc >= coglimit) {
                ERROR(ast, "COG PC of $%lx exceeds limit of $%lx\n", (long)cogpc/4, (long)coglimit/4);
                coglimit = 0;
            }
        }
        if (ast->kind == AST_ASM_IF) {
            int val;
            bool was_active = state[asm_nest].is_active;
            asm_nest++;
            if (asm_nest == MAX_ASM_NEST) {
                ERROR(ast, "conditional assembly nested too deep");
                --asm_nest;
            }
            if (!IsConstExpr(ast->left)) {
                ERROR(ast, "expression in conditional assembly must be constant");
                val = 0;
            } else {
                val = EvalConstExpr(ast->left);
            }
            if (val && was_active) {
                state[asm_nest].is_active = true;
                state[asm_nest].needs_else = false;
                ast->d.ival = 1;
            } else {
                state[asm_nest].is_active = false;
                state[asm_nest].needs_else = was_active;
                ast->d.ival = 0;
            }
        } else if (ast->kind == AST_ASM_ELSEIF) {
            int val;
            if (!IsConstExpr(ast->left)) {
                ERROR(ast, "expression in conditional assembly must be constant");
                val = 0;
            } else {
                val = EvalConstExpr(ast->left);
            }
            if (state[asm_nest].needs_else && val) {
                state[asm_nest].needs_else = false;
                state[asm_nest].is_active = true;
                ast->d.ival = 1;
            } else {
                state[asm_nest].is_active = false;
                ast->d.ival = 0;
            }
        } else if (ast->kind == AST_ASM_ENDIF) {
            if (asm_nest == 0) {
                ERROR(ast, "conditional assembly endif without if");
            } else {
                --asm_nest;
            }
            ast->d.ival = state[asm_nest].is_active ? 1 : 0;
        } else if (state[asm_nest].is_active) {
        switch (ast->kind) {
        case AST_BYTELIST:
        case AST_BYTEFITLIST:
            MARK_DATA(label_flags);
            pendingLabels = emitPendingLabels(symtab, pendingLabels, hubpc, cogpc, ast_type_byte, lastOrg, inHub, label_flags, pass);
            replaceHereDataList(ast->left, inHub, inHub ? hubpc : cogpc, 1, lastOrg);
            INCPC(dataListLen(ast->left, 1));
            lasttype = ast_type_byte;
            break;
        case AST_WORDLIST:
        case AST_WORDFITLIST:
            MARK_DATA(label_flags);
            MAYBEALIGNPC(2);
            pendingLabels = emitPendingLabels(symtab, pendingLabels, hubpc, cogpc, ast_type_word, lastOrg, inHub, label_flags, pass);
            replaceHereDataList(ast->left, inHub, inHub ? hubpc : cogpc, 2, lastOrg);
            INCPC(dataListLen(ast->left, 2));
            lasttype = ast_type_word;
            break;
        case AST_LONGLIST:
            MARK_DATA(label_flags);
            MAYBEALIGNPC(4);
            pendingLabels = emitPendingLabels(symtab, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub, label_flags, pass);
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
                pendingLabels = emitPendingLabels(symtab, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub, label_flags | LABEL_HAS_JMP, pass);
            } else {
                pendingLabels = emitPendingLabels(symtab, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub, label_flags, pass);
            }
            replaceHeres(ast->left, HEREPC, lastOrg);
            ast->d.ival = inHub ? hubpc : (cogpc | (1<<30));
            INCPC(InstrSize(ast->left));
            lasttype = ast_type_long;
            current->datHasCode = 1;
            break;
        case AST_BRKDEBUG:
            if (!gl_brkdebug) WARNING(ast,"Internal error, Got AST_BRKDEBUG, but BRK debugger is not enabled?");
            MARK_CODE(label_flags);
            // Kinda similiar the instruction code above ..
            if (inHub) {
                MAYBEALIGNPC(4);
            } else {
                ALIGNPC(4);
            }
            pendingLabels = emitPendingLabels(symtab, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub, label_flags, pass);
            replaceHeres(ast->left, HEREPC, lastOrg);
            INCPC(4); // BRK is always 4 byte
            lasttype = ast_type_long;
            current->datHasCode = 1;
            break;
        case AST_IDENTIFIER:
            pendingLabels = AddToList(pendingLabels, NewAST(AST_LISTHOLDER, ast, NULL));
            break;
        case AST_ORG:
            MAYBEALIGNPC(4);
            pendingLabels = emitPendingLabels(symtab, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub, label_flags, pass);
            if (ast->left) {
                AST *addr = ast->left;
                if (addr->kind == AST_RANGE) {
                    coglimit = 4 * EvalPasmExpr(addr->right);
                    addr = addr->left;
                } else {
                    coglimit = 0;
                }
                replaceHeres(addr, HEREPC, lastOrg);
                cogpc = 4*EvalPasmExpr(addr);
            } else {
                cogpc = 0;
            }
            tmpName = NewOrgName();
            lastOrg = AddInternalSymbol(symtab, tmpName, SYM_CONSTANT, AstInteger(cogpc), NULL);
            lasttype = ast_type_long;
            ast->d.ptr = (void *)lastOrg;
            inHub = 0;
            MARK_COG(label_flags);
            break;
        case AST_ORGH:
            pendingLabels = emitPendingLabels(symtab, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub, label_flags, pass);
            ast->d.ival = hubpc; // temporary
            if (ast->left) {
                AST *addr = ast->left;
                if (addr->kind == AST_RANGE) {
                    hublimit = EvalPasmExpr(addr->right);
                    addr = addr->left;
                } else {
                    hublimit = 0;
                }
                replaceHeres(addr, hubpc, lastOrg);
                hubpc = EvalPasmExpr(addr);
            }
            cogpc = hubpc;
            // calculate padding amount
            if (ast->d.ival <= hubpc) {
                ast->d.ival = hubpc - ast->d.ival;
            } else {
                ERROR(ast, "orgh address %lx is less than previous address %lx",
                      (long)hubpc, (unsigned long)ast->d.ival);
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
                AST *addr = ast->left;
                replaceHeres(addr, HEREPC, lastOrg);
                uint32_t newpc = 4*EvalPasmExpr(addr);
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
            pendingLabels = emitPendingLabels(symtab, pendingLabels, hubpc, cogpc, ast_type_void, lastOrg, inHub, label_flags, pass);
            replaceHeres(ast->left, cogpc / 4, lastOrg);
            delta = EvalPasmExpr(ast->left);
            if ( ((int)delta) < 0) {
                ERROR(ast, "RES with negative storage specifier");
                delta = 0;
            } else if (delta == 0) {
                // don't issue warning for alignment or if the user gave an explicit 0
                // unfortunately at this point we've already replaced $, so
                // we have to guess at intention by looking for &
                if (ast && ast->left && ast->left->kind == AST_OPERATOR && ast->left->d.ival == '&') {
                    // no warning
                } else if (ast && ast->left && ast->left->kind == AST_INTEGER && ast->left->d.ival == 0) {
                    // no warning
                } else {
                    // suppress this warning the second time around
                    if (!(gl_warn_flags & WARN_ASM_FIRST_PASS)) {
                        WARNING(ast, "RES has 0 space");
                    }
                }
            }
            cogpc += 4*delta;
//            hubpc += 4*delta;
            lasttype = ast_type_long;
            break;
        case AST_FIT:
            cogpc = align(cogpc, 4);
            pendingLabels = emitPendingLabels(symtab, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub, label_flags, pass);
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
            pendingLabels = emitPendingLabels(symtab, pendingLabels, hubpc, cogpc, ast_type_byte, lastOrg, inHub, label_flags, pass);
            INCPC(filelen(ast->left));
            break;
        case AST_LINEBREAK:
            pendingLabels = emitPendingLabels(symtab, pendingLabels, hubpc, cogpc, lasttype, lastOrg, inHub, label_flags, pass);
            break;
        case AST_COMMENT:
        case AST_SRCCOMMENT:
            break;
        case AST_ALIGN:
            pendingLabels = emitPendingLabels(symtab, pendingLabels, hubpc, cogpc, lasttype, lastOrg, inHub, label_flags, pass);
            ALIGNPC(EvalPasmExpr(ast->left));
            break;
        case AST_DECLARE_VAR:
            {
                AST *type = ast->left;
                AST *ident = ast->right;
                AST *initializer = NULL;
                AST **initptr = NULL;
                ASTReportInfo saveinfo;
                int typalign;
                int typsize;

                // initializers always go in HUB memory
                inHub = 1;
                MARK_DATA(label_flags);
                AstReportAs(ident, &saveinfo);
                if (ident->kind == AST_ASSIGN) {
                    initptr = &ident->right;
                    initializer = ident->right;
                    ident = ident->left;
                }
                while (ident && ident->kind == AST_ARRAYDECL) {
                    type = MakeArrayType(type, ident->right);
                    ident = ident->left;
                }
                typalign = TypeAlign(type);
                typsize = TypeSize(type);
                if (typsize == 0 && IsClassType(type)) {
                    // empty object; this is OK if it has methods
                    Module *Q = GetClassPtr(type);
                    if (Q && !Q->functions) {
                        ERROR(ast, "empty or undefined class used to define %s", GetUserIdentifierName(ident));
                    }
                }
                if (initptr && (IsClassType(type) || IsArrayType(type)) ) {
                    initializer = FixupInitList(type, initializer);
                    *initptr = initializer;
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
                pendingLabels = emitPendingLabels(symtab, pendingLabels, hubpc, cogpc, type, lastOrg, inHub, label_flags, pass);
                INCPC(typsize);
                
                fixupInitializer(current, initializer, type);
                AstReportDone(&saveinfo);
            }
            break;
        default:
            ERROR(ast, "unknown element %d in data block", ast->kind);
            break;
        }
    }
    }
    
    // emit any labels that come at the end
    pendingLabels = emitPendingLabels(symtab, pendingLabels, hubpc, cogpc, lasttype, lastOrg, inHub, label_flags, pass);

    if (found_undefined_labels) {
        expect_undefined_labels = found_undefined_labels = 0;
        orig_datoff = datoff;
        goto again;
    }
    if (orig_datoff && (labels_changed && datoff <= orig_datoff)) {
        orig_datoff = datoff;
        goto again;
    }
    return datoff;
}

void
DeclareModuleLabels(Module *P)
{
    Module *save = current;
    int startFlags = 0;

    if (gl_no_coginit && gl_output == OUTPUT_DAT) {
        startFlags = ADDRESS_STARTFLAG_HUB;
    }
    current = P;
    P->gasPasm = gl_gas_dat;
    P->datsize = AssignAddresses(&P->objsyms, P->datblock, startFlags);
    current = save;
}
