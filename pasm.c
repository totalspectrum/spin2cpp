/*
 * PASM and data compilation
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "spinc.h"

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
 * find the length of a data list, in bytes
 */
unsigned
dataListLen(AST *ast, int elemsize)
{
    unsigned size = 0;
    unsigned numelems;
    AST *sub;

    while (ast) {
        sub = ast->left;
        if (sub) {
            if (sub->kind == AST_ARRAYDECL || sub->kind == AST_ARRAYREF) {
                numelems = EvalPasmExpr(ast->left->right);
            } else if (sub->kind == AST_STRING) {
                numelems = strlen(sub->d.string);
            } else if (sub->kind == AST_RANGE) {
                int start = EvalPasmExpr(sub->left);
                numelems = (EvalPasmExpr(sub->right) - start) + 1;
                if ((int)numelems < 0) {
                    ERROR(sub, "Backwards range not supported");
                    numelems = 0;
                }
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
EnterLabel(Module *P, AST *origLabel, long hubpc, long cogpc, AST *ltype, Symbol *lastorg, int inHub)
{
    const char *name;
    Label *labelref;
    Symbol *sym;

    name = origLabel->d.string;
    sym = FindSymbol(&P->objsyms, name);
    if (sym) {
        // redefining a label with the exact same values is OK
        if (sym->type != SYM_LABEL) {
            ERROR(origLabel, "Redefining symbol %s", name);
            return;
        }
        labelref = (Label *)sym->val;
#if 0        
        if (labelref->hubval != hubpc) {
            ERROR(origLabel, "Changing hub value for symbol %s", name);
            return;
        }
        if (labelref->cogval != cogpc) {
            ERROR(origLabel, "Changing cog value for symbol %s", name);
            return;
        }
        if (labelref->org != lastorg) {
            ERROR(origLabel, "Changing lastorg value for symbol %s", name);
            return;
        }
        if (!CompatibleTypes(labelref->type, ltype)) {
            ERROR(origLabel, "Changing type of symbol %s", name);
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
    if (inHub) {
        labelref->flags = LABEL_IN_HUB;
    }
    if (!sym) {
        sym=AddSymbol(&P->objsyms, name, SYM_LABEL, labelref);
    }
}

/*
 * emit pending labels
 */
AST *
emitPendingLabels(Module *P, AST *label, unsigned hubpc, unsigned cogpc, AST *ltype, Symbol *lastorg, int inHub)
{
    while (label) {
        EnterLabel(P, label->left, hubpc, cogpc, ltype, lastorg, inHub);
//        printf("label: %s = %x\n", label->left->d.string, hubpc);
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
    if (!exprlist || exprlist->kind != AST_EXPRLIST) {
        ERROR(exprlist, "internal error, expected exprlist");
        return exprlist;
    }
    while (exprlist) {
        elem = exprlist->left;
        next = exprlist->right;
        if (elem->kind == AST_STRING) {
            AST *ast;
            const char *t = elem->d.string;
            int c;
            first = elem;
            do {
                c = *t++;
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
        }
        exprlist = next;
    }
    return orig_exprlist;
}

static void
fixupInitializer(Module *P, AST *initializer, AST *type)
{
    AST *newinit;
    AST *newident;
    AST *subtype;
    AST *elem;
    type = RemoveTypeModifiers(type);
    if (!type) {
        return;
    }
    if (!initializer) {
        return;
    }
    if (initializer->kind == AST_STRINGPTR) {
        *initializer = *reduceStrings(initializer->left);
    }
    if (type->kind == AST_PTRTYPE) {
        if (initializer->kind == AST_STRINGPTR) {
            elem = initializer->left;
        } else {
            elem = initializer;
        }
        if (elem->kind == AST_EXPRLIST) {
            AST *ast, *declare;

            /* need to move it to its own declaration */
            subtype = type->left;
            newinit = NewAST(AST_EXPRLIST, NULL, NULL);
            *newinit = *elem;
            newident = AstTempIdentifier("_array_");
            declare = AstAssign(newident, newinit);

            subtype = NewAST(AST_ARRAYTYPE, subtype, AstInteger(AstListLen(newinit)));
            declare = NewAST(AST_DECLARE_VAR, subtype, declare);
            ast = NewAST(AST_COMMENTEDNODE, declare, NULL);
            P->datblock = AddToList(P->datblock, ast);
            *initializer = *NewAST(AST_ABSADDROF, newident, NULL);

            /* add newident to P */
        }
    }
    if (type->kind == AST_ARRAYTYPE) {
        AST *elem;
        subtype = type->left;
        if (initializer->kind != AST_EXPRLIST) {
            ERROR(initializer, "wrong kind of initializer for array type");
            return;
        }
        for (elem = initializer; elem; elem = elem->right) {
            fixupInitializer(P, elem->left, subtype);
        }
    }
}

/*
 * declare labels for a data block
 */

#define ALIGNPC(size)  do { inc = size; cogpc = align(cogpc, inc); datoff = align(datoff, inc); hubpc = align(hubpc, inc); } while (0)
#define MAYBEALIGNPC(size) if (!gl_p2) { ALIGNPC(size); }
#define INCPC(size)  do { inc = size; cogpc += inc; datoff += inc; hubpc += inc; } while (0)
#define HEREPC (inHub ? hubpc : (cogpc/4))

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

    P->gasPasm = gl_gas_dat;
    if (gl_no_coginit && gl_output == OUTPUT_DAT) {
        // insert an implicit orgh P2_HUB_BASE
        hubpc = gl_hub_base;
        inHub = 1;
    }
    for (top = P->datblock; top; top = top->right) {
        ast = top;
        while (ast && ast->kind == AST_COMMENTEDNODE) {
            ast = ast->left;
        }
        if (!ast) continue;
        switch (ast->kind) {
        case AST_BYTELIST:
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_byte, lastOrg, inHub);
            replaceHereDataList(ast->left, inHub, inHub ? hubpc : cogpc, 1, lastOrg);
            INCPC(dataListLen(ast->left, 1));
            lasttype = ast_type_byte;
            break;
        case AST_WORDLIST:
            MAYBEALIGNPC(2);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_word, lastOrg, inHub);
            replaceHereDataList(ast->left, inHub, inHub ? hubpc : cogpc, 2, lastOrg);
            INCPC(dataListLen(ast->left, 2));
            lasttype = ast_type_word;
            break;
        case AST_LONGLIST:
            MAYBEALIGNPC(4);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub);
            replaceHereDataList(ast->left, inHub, inHub ? hubpc : cogpc, 4, lastOrg);
            INCPC(dataListLen(ast->left, 4));
            lasttype = ast_type_long;
            break;
        case AST_INSTRHOLDER:
            MAYBEALIGNPC(4);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub);
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
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub);
            if (ast->left) {
                replaceHeres(ast->left, HEREPC, lastOrg);
                cogpc = 4*EvalPasmExpr(ast->left);
            } else {
                cogpc = 0;
            }
            tmpName = NewOrgName();
            lastOrg = AddSymbol(&current->objsyms, tmpName, SYM_CONSTANT, AstInteger(cogpc));
            lasttype = ast_type_long;
            ast->d.ptr = (void *)lastOrg;
            inHub = 0;
            break;
        case AST_ORGH:
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub);
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
            cogpc = align(cogpc, 4);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub);
            delta = EvalPasmExpr(ast->left);
            cogpc += 4*delta;
//            hubpc += 4*delta;
            lasttype = ast_type_long;
            break;
        case AST_FIT:
            cogpc = align(cogpc, 4);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_long, lastOrg, inHub);
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
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, ast_type_byte, lastOrg, inHub);
            INCPC(filelen(ast->left));
            break;
        case AST_LINEBREAK:
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, lasttype, lastOrg, inHub);
            break;
        case AST_COMMENT:
        case AST_SRCCOMMENT:
            break;
        case AST_ALIGN:
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, lasttype, lastOrg, inHub);
            ALIGNPC(EvalPasmExpr(ast->left));
            break;
        case AST_DECLARE_VAR:
            {
                AST *type = ast->left;
                AST *ident = ast->right;
                AST *initializer = NULL;
                int typalign;
                int typsize;
                if (ident->kind == AST_ASSIGN) {
                    initializer = ident->right;
                    ident = ident->left;
                }
                while (ident && ident->kind == AST_ARRAYDECL) {
                    type = NewAST(AST_ARRAYTYPE, type, ident->right);
                    ident = ident->left;
                }
                typalign = TypeAlign(type);
                typsize = TypeSize(type);
                MAYBEALIGNPC(typalign);
                if (ident->kind != AST_IDENTIFIER) {
                    ERROR(ast, "Internal error in DECLARE_VAR: expected identifier");
                } else {
                    pendingLabels = AddToList(pendingLabels, NewAST(AST_LISTHOLDER, ident, NULL));
                }
                pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, type, lastOrg, inHub);
                INCPC(typsize);
                fixupInitializer(P, initializer, type);
            }
            break;
        default:
            ERROR(ast, "unknown element %d in data block", ast->kind);
            break;
        }
    }

    // emit any labels that come at the end
    pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, cogpc, lasttype, lastOrg, inHub);
    
    P->datsize = datoff;
}
