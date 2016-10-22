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
    InstrModifier *im;
    while(instr) {
        ast = instr;
        instr = instr->right;
        if (ast && ast->kind == AST_EXPRLIST) {
            ast = ast->left;
        }
        if (ast && ast->kind == AST_INSTRMODIFIER) {
            im = (InstrModifier *)ast->d.ptr;
            if (!strcmp(im->name, "##")) {
                size += 4;
            }
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
                if (numelems < 0) {
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
EnterLabel(Module *P, AST *origLabel, long offset, long asmpc, AST *ltype, Symbol *lastorg)
{
    const char *name;
    Label *labelref;

    labelref = (Label *)calloc(1, sizeof(*labelref));
    if (!labelref) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    name = origLabel->d.string;
    labelref->offset = offset;
    labelref->asmval = asmpc;
    labelref->type = ltype;
    labelref->org = lastorg;
    if (!AddSymbol(&P->objsyms, name, SYM_LABEL, labelref)) {
      ERROR(origLabel, "Duplicate definition of label %s", name);
    }
}

/*
 * emit pending labels
 */
AST *
emitPendingLabels(Module *P, AST *label, unsigned pc, unsigned asmpc, AST *ltype, Symbol *lastorg)
{
    while (label) {
        EnterLabel(P, label->left, pc, asmpc, ltype, lastorg);
        label = label->right;
    }
    return NULL;
}

/*
 * replace AST_HERE with AST_INTEGER having pc as its value, if necessary
 * otherwise update the AST_HERE with the value of the last origin symbol
 */
void
replaceHeres(AST *ast, uint32_t asmpc, Symbol *lastOrg)
{
    if (ast == NULL)
        return;
    if (ast->kind == AST_HERE) {
        if (gl_gas_dat) {
            ast->d.ptr = (void *)lastOrg;
        } else {
            ast->kind = AST_INTEGER;
            ast->d.ival = asmpc;
        }
        return;
    }
    replaceHeres(ast->left, asmpc, lastOrg);
    replaceHeres(ast->right, asmpc, lastOrg);
}

/*
 * do replaceHeres on each element of a long list
 */
void
replaceHereDataList(AST *ast, uint32_t asmpc, uint32_t inc, Symbol *lastOrg)
{
    AST *sub;

    while (ast) {
        sub = ast->left;
        if (sub)
            replaceHeres(sub, asmpc/4, lastOrg);
        asmpc += inc;
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

/*
 * declare labels for a data block
 */

#define ALIGNPC(size)  do { inc = size; asmpc = align(asmpc, inc); datoff = align(datoff, inc); hubpc = align(hubpc, inc); } while (0)
#define MAYBEALIGNPC(size) if (!gl_p2) { ALIGNPC(size); }
#define INCPC(size)  do { inc = size; asmpc += inc; datoff += inc; hubpc += inc; } while (0)

void
DeclareLabels(Module *P)
{
    unsigned asmpc = 0;
    unsigned hubpc = 0;
    unsigned datoff = 0;
    unsigned inc = 0;
    unsigned delta;

    AST *top = NULL;
    AST *ast = NULL;
    AST *pendingLabels = NULL;
    AST *lasttype = ast_type_long;
    Symbol *lastOrg = NULL;
    const char *tmpName;

    P->gasPasm = gl_gas_dat;
    for (top = P->datblock; top; top = top->right) {
        ast = top;
        while (ast && ast->kind == AST_COMMENTEDNODE) {
            ast = ast->left;
        }
        if (!ast) continue;
        switch (ast->kind) {
        case AST_BYTELIST:
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, asmpc, ast_type_byte, lastOrg);
            replaceHereDataList(ast->left, asmpc, 1, lastOrg);
            INCPC(dataListLen(ast->left, 1));
            lasttype = ast_type_byte;
            break;
        case AST_WORDLIST:
            MAYBEALIGNPC(2);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, asmpc, ast_type_word, lastOrg);
            replaceHereDataList(ast->left, asmpc, 2, lastOrg);
            INCPC(dataListLen(ast->left, 2));
            lasttype = ast_type_word;
            break;
        case AST_LONGLIST:
            MAYBEALIGNPC(4);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, asmpc, ast_type_long, lastOrg);
            replaceHereDataList(ast->left, asmpc, 4, lastOrg);
            INCPC(dataListLen(ast->left, 4));
            lasttype = ast_type_long;
            break;
        case AST_INSTRHOLDER:
            MAYBEALIGNPC(4);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, asmpc, ast_type_long, lastOrg);
            replaceHeres(ast->left, asmpc/4, lastOrg);
            ast->d.ival = asmpc;
            INCPC(InstrSize(ast->left));
            lasttype = ast_type_long;
            current->datHasCode = 1;
            break;
        case AST_IDENTIFIER:
            pendingLabels = AddToList(pendingLabels, NewAST(AST_LISTHOLDER, ast, NULL));
            break;
        case AST_ORG:
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, asmpc, ast_type_long, lastOrg);
            if (ast->left) {
                replaceHeres(ast->left, asmpc/4, lastOrg);
                asmpc = 4*EvalPasmExpr(ast->left);
            } else {
                asmpc = 0;
            }
            tmpName = NewOrgName();
            lastOrg = AddSymbol(&current->objsyms, tmpName, SYM_CONSTANT, AstInteger(asmpc));
            lasttype = ast_type_long;
            ast->d.ptr = (void *)lastOrg;
            break;
        case AST_ORGH:
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, asmpc, ast_type_long, lastOrg);
            ast->d.ival = asmpc;
            if (ast->left) {
                replaceHeres(ast->left, hubpc, lastOrg);
                hubpc = EvalPasmExpr(ast->left);
            } else {
                hubpc = 0;
            }
            asmpc = hubpc;
            lasttype = ast_type_long;
            break;
        case AST_RES:
            asmpc = align(asmpc, 4);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, asmpc, ast_type_long, lastOrg);
            delta = EvalPasmExpr(ast->left);
            asmpc += 4*delta;
//            hubpc += 4*delta;
            lasttype = ast_type_long;
            break;
        case AST_FIT:
            asmpc = align(asmpc, 4);
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, asmpc, ast_type_long, lastOrg);
            if (ast->left) {
                int32_t max = EvalPasmExpr(ast->left);
                int32_t cur = (asmpc) / 4;
                if ( cur > max ) {
                    ERROR(ast, "fit %d failed: pc is %d", max, cur);
                }
            }
            lasttype = ast_type_long;
            break;
        case AST_FILE:
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, asmpc, ast_type_byte, lastOrg);
            INCPC(filelen(ast->left));
            break;
        case AST_LINEBREAK:
            pendingLabels = emitPendingLabels(P, pendingLabels, hubpc, asmpc, lasttype, lastOrg);
            break;
        case AST_COMMENT:
            break;
        default:
            ERROR(ast, "unknown element %d in data block", ast->kind);
            break;
        }
    }

    P->datsize = datoff;
}
