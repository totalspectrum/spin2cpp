//
// Bytecode (nucode) compiler for spin2cpp
//
// Copyright 2021-2022 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include "outnu.h"
#include "becommon.h"
#include <stdlib.h>

/* some utility functions */
void NuDeleteIr(NuIrList *irl, NuIr *ir) {
    NuIr *prev = ir->prev;
    NuIr *next = ir->next;
    
    if (prev) {
        prev->next = next;
    } else {
        irl->head = next;
    }
    if (next) {
        next->prev = prev;
    } else {
        irl->tail = prev;
    }
}

void NuIrInsertBefore(NuIrList *irl, NuIr *anchor, NuIr *newitem) {
    NuIr *prev = anchor ? anchor->prev : NULL;
    if (anchor == NULL) {
        // append to end of list
        newitem->prev = irl->tail;
        newitem->next = NULL;
        irl->tail = newitem;
    } else {
        newitem->prev = prev;
        newitem->next = anchor;
        anchor->prev = newitem;
        if (prev) {
            prev->next = newitem;
        } else {
            irl->head = newitem;
        }
    }
}

/* size of jump buffer for Nu code */
#define NU_JMP_BUF_SIZ (9*LONG_SIZE)

#define NumRetLongs(F) FuncLongResults(F->overalltype)
#define NumArgLongs(F) FuncLongParams(F->overalltype)
#define NumLocalLongs(F) ((F)->numlocals)

static void NuCompileStatement(NuIrList *irl, AST *ast); // forward declaration
static void NuCompileStmtlist(NuIrList *irl, AST *ast); // forward declaration
static int NuCompileExpression(NuIrList *irl, AST *ast); // returns number of items left on stack
static int NuCompileExprList(NuIrList *irl, AST *ast); // returns number of items left on stack
static NuIrOpcode NuCompileLhsAddress(NuIrList *irl, AST *lhs); // returns op used for storing
static int NuCompileMul(NuIrList *irl, AST *lhs, AST *rhs, int gethi);

static AST *nu_stack_ptr = 0;
static AST *nu_abortchain_ptr = 0;
static AST *nu_abortresult_ptr = 0;

typedef struct NuLabelList {
    struct NuLabelList *next;
    NuIrLabel *label;
} NuLabelList;

static NuLabelList quitstack;
static NuIrLabel *nextlabel, *quitlabel;

static void NuPushQuitNext(NuIrLabel *q, NuIrLabel *n) {
    NuLabelList *qholder = (NuLabelList *)malloc(sizeof(NuLabelList));
    NuLabelList *nholder = (NuLabelList *)malloc(sizeof(NuLabelList));
    qholder->label = quitlabel;
    nholder->label = nextlabel;
    qholder->next = nholder;
    nholder->next = quitstack.next;
    quitstack.next = qholder;

    quitlabel = q;
    nextlabel = n;
}
static void NuPopQuitNext() {
    NuLabelList *ql, *nl;
    ql = quitstack.next;
    if (!ql || !ql->next) {
        ERROR(NULL, "Internal error, empty loop stack");
        return;
    }
    nl = ql->next;
    quitstack.next = nl->next;
    quitlabel = ql->label;
    nextlabel = nl->label;
    free(nl);
    free(ql);    
}

static NuIrLabel *
NuIrOffsetLabel(NuIrLabel *base, int offset) {
    NuIrLabel *r = malloc(sizeof(*r));
    *r = *base;
    r->offset = offset;
    return r;
}

static NuIrLabel *NuGetLabelFromSymbol(AST *where, const char *name) {
    Symbol *sym = FindSymbol(&curfunc->localsyms, name);
    if (!sym || sym->kind != SYM_LOCALLABEL) {
        ERROR(where, "%s is not a label in this function", name);
        return NULL;
    }
    if (!sym->val) {
        sym->val = (void *)NuCreateLabel();
    }
    return (NuIrLabel *)sym->val;
}

static void
NuPrepareModuleBedata(Module *P) {
    // Init bedata
    if (P->bedata) return;

    DEBUG(NULL,"Preparing object %s",P->fullname);

    P->bedata = calloc(sizeof(NuModData), 1);
    ModData(P)->datLabel = NuCreateLabel();
    ModData(P)->isCompiled = false;
}

static void
NuPrepareFunctionBedata(Function *F) {
    if (F->bedata) {
        return;
    }
    F->bedata = calloc(sizeof(NuFunData),1);
    
    FunData(F)->entryLabel = NuCreateLabel();
    //FunData(F)->exitLabel = NuCreateLabel();    
}

const char *NuCodeSymbolName(Symbol *sym) {
    if (sym->kind == SYM_FUNCTION) {
        Function *F = (Function *)sym->val;
        NuPrepareFunctionBedata(F);
        return NuLabelName(FunData(F)->entryLabel);
    }
    ERROR(NULL, "cannot handle symbol type");
    return "???";
}

// drop n items from the stack
static void NuCompileDrop(NuIrList *irl, int n) {
    while (n > 1) {
        NuEmitOp(irl, NU_OP_DROP2);
        n -= 2;
    }
    if (n) {
        NuEmitOp(irl, NU_OP_DROP);
    }
}

static int NuCompileFunCall(NuIrList *irl, AST *node) {
    int pushed;
    Function *func = NULL;
    AST *functype = NULL;
    AST *objref = NULL;
    Symbol *sym;
    AST *args = node->right;
    NuIr *ir;
    
    pushed = NuCompileExprList(irl, args);
    sym = FindFuncSymbol(node, &objref, 1);
    if (sym && sym->kind == SYM_FUNCTION) {
        func = (Function *)sym->val;
        pushed = func->numresults;
        if (!func->body) {
            if (pushed) {
                ERROR(node, "empty function with results expected...");
            }
            return 0;
        }
        if (func->body->kind == AST_BYTECODE) {
            ir = NuEmitNamedOpcode(irl, func->body->d.string);
            if (!func->result_declared) {
                pushed = 0;
            }
        } else {
            // output a CALL
            NuPrepareFunctionBedata(func);
            if (func->is_static || func->module == current) {
                // plain CALL is OK
                NuEmitCall(irl, NU_OP_CALLA, FunData(func)->entryLabel, auto_printf(128, "call %s", func->name));
            } else if (func->module == systemModule) {
                // system modules don't actually have non-static functions, we're just
                //WARNING(node, "non-static system module function called");
                // plain CALL is OK
                NuEmitCall(irl, NU_OP_CALLA, FunData(func)->entryLabel, auto_printf(128, "call %s", func->name));
            } else if (objref) {
                // compile a method call
                NuCompileLhsAddress(irl, objref);
                NuEmitCommentedAddress(irl, FunData(func)->entryLabel, func->name);
                ir = NuEmitOp(irl, NU_OP_CALLM);
                ir->comment = auto_printf(128, "call method %s", func->name);                
            } else {
                ERROR(node, "Unable to compile this method calls");
            }
        }
    } else if (node->left && IsIdentifier(node->left) && !LookupAstSymbol(node->left, NULL)) {
            ERROR(node, "Unknown symbol %s", GetUserIdentifierName(node->left));
    } else {
        AST *funcNode = node->left;
        functype = ExprType(funcNode);
        // FIXME: handle weird problem with (*f)() vs f(), which should
        // mean the same thing
        if (funcNode->kind == AST_ARRAYREF && IsConstZero(funcNode->right)) {
            AST *arr = funcNode->left;
            if (arr->kind ==  AST_MEMREF && IsFunctionType(arr->left)) {
                funcNode = arr->right;
            }
        }
        pushed = NuCompileExpression(irl, funcNode);
        if (pushed != 1) {
            ERROR(node, "Unable to compile indirect function call: %d items on stack", pushed);
        }
        // at this point the method pointer words are on the stack; need to load both words
        NuEmitCommentedOp(irl, NU_OP_LDD, "fetch pc and objptr");
        NuEmitCommentedOp(irl, NU_OP_CALLM, "indirect call");
        pushed = FuncLongResults(functype);
    }
    return pushed;
}

// find the load opcode corresponding to a store opcode
static NuIrOpcode NuLoadOpFor(NuIrOpcode stOp) {
    switch (stOp) {
    case NU_OP_STB: return NU_OP_LDB;
    case NU_OP_STW: return NU_OP_LDW;
    case NU_OP_STL: return NU_OP_LDL;
    case NU_OP_STD: return NU_OP_LDD;
    case NU_OP_STREG: return NU_OP_LDREG;
    default:
        ERROR(NULL, "bad kind of store op");
        return NU_OP_ILLEGAL;
    }
}

// find how to load a particular type, or return NU_OP_ILLEGAL if we don't know how
static NuIrOpcode LoadStoreOp(AST *typ, int isLoad)
{
    int siz;
    NuIrOpcode op = NU_OP_ILLEGAL;

    if (IsArrayType(typ)) {
        typ = BaseType(typ);
    }    
    siz = TypeSize(typ);
    switch (siz) {
    case 0:
        //ERROR(NULL, "Attempt to use void value"); // this is OK if "typ" is an empty object
        return op;
    case 1:
        op = isLoad ? NU_OP_LDB : NU_OP_STB;
        break;
    case 2:
        op = isLoad ? NU_OP_LDW : NU_OP_STW;
        break;
    case 4:
        op = isLoad ? NU_OP_LDL : NU_OP_STL;
        break;
    case 8:
        op = isLoad ? NU_OP_LDD : NU_OP_STD;
        break;
    default:
        break;
    }
    return op;
}

/* push effective address of an identifier onto the stack */
/* returns an opcode for how to load this identifier, if we know */
static NuIrOpcode
NuCompileIdentifierAddress(NuIrList *irl, AST *node, int isLoad)
{
    Symbol *sym = LookupAstSymbol(node,NULL);
    NuIrOpcode offsetOp = NU_OP_ADD_DBASE;
    NuIrOpcode loadOp = NU_OP_ILLEGAL;
    NuIr *ir;
    const char *name = NULL;
    int offset = 0;
    bool offsetValid = true;
    if (sym && sym->kind == SYM_WEAK_ALIAS) {
        sym = LookupSymbol((const char *)sym->val);
    }
    if (!sym) {
        ERROR(node, "identifier %s not found", GetUserIdentifierName(node));
        return isLoad ? NU_OP_LDL : NU_OP_STL;
    }
    name = sym->user_name;
    if (sym->kind == SYM_RESERVED && !strcasecmp(name, "result")) {
        if (curfunc->resultexpr) {
            sym = LookupAstSymbol(curfunc->resultexpr, NULL);
        } else {
            sym = 0;
        }
        if (!sym) {
            ERROR(node, "internal error, unable to find RESULT");
            return loadOp;
        }
    }
    switch (sym->kind) {
    case SYM_LOCALVAR:
    case SYM_TEMPVAR:
    case SYM_PARAMETER:
    case SYM_RESULT:
        offset = sym->offset;
        if (curfunc->closure) {
            offsetOp = NU_OP_ADD_VBASE;
        }
        loadOp = LoadStoreOp(sym->val, isLoad);
        break;
    case SYM_CLOSURE:
        offset = sym->offset;
        offsetOp = NU_OP_ADD_VBASE;
        loadOp = LoadStoreOp(sym->val, isLoad);
        break;
    case SYM_VARIABLE:
        loadOp = LoadStoreOp(sym->val, isLoad);
        if (sym->flags & SYMF_GLOBAL) {
            offset = sym->offset;
            if (offset < 0) {
                switch (offset) {
                case -1: // sendptr
                    offset = 0x1e8;
                    loadOp = (isLoad) ? NU_OP_LDREG : NU_OP_STREG;
                    break;
                case -2: // recvptr
                    offset = 0x1e9;
                    loadOp = (isLoad) ? NU_OP_LDREG : NU_OP_STREG;
                    break;
                default:
                    offset = -offset;
                    loadOp = (isLoad) ? NU_OP_LDREG : NU_OP_STREG;
                    break;
                }
            }
            offsetOp = NU_OP_ILLEGAL;
        } else {
            offset = sym->offset;
            if (curfunc->closure) {
                offsetOp = NU_OP_ADD_SUPER;
            } else {
                offsetOp = NU_OP_ADD_VBASE;
            }
        }
        break;
    case SYM_LABEL:
    {
        Label *lab = sym->val;
        NuIrLabel *nulabel = NULL;
        uint32_t labelval = lab->hubval;
        Module *Q = sym->module;
        if (!Q) {
            Q = current;
        }
        offset = labelval;
        loadOp = LoadStoreOp(lab->type, isLoad);
        if (!ModData(Q)) {
            NuPrepareModuleBedata(Q);
        }
        nulabel = ModData(Q)->datLabel;
        nulabel = NuIrOffsetLabel(nulabel, offset);
        ir = NuEmitCommentedAddress(irl, nulabel, name);
        return loadOp;
    }
    case SYM_HWREG: {
        HwReg *hwreg = (HwReg *)sym->val;
        offset = hwreg->addr;
        loadOp = isLoad ? NU_OP_LDREG : NU_OP_STREG;
        offsetOp = NU_OP_ILLEGAL;
    } break;
    case SYM_FUNCTION: {
        Function *F = (Function *)sym->val;
        NuPrepareFunctionBedata(F);
        NuEmitCommentedAddress(irl, FunData(F)->entryLabel, F->name);
        loadOp = isLoad ? NU_OP_LDL : NU_OP_STL;
        offsetOp = NU_OP_ILLEGAL;
        offsetValid = false;
    } break;
    case SYM_ALIAS: {
        AST *expr = (AST *)sym->val;
        // this had better be a simple alias
        while (expr && expr->kind == AST_CAST) {
            expr = expr->right;
        }
        if (!expr) {
            ERROR(node, "Unable to compile aliased expression");
            return loadOp;
        }
        loadOp = NuCompileLhsAddress(irl, expr);
        if (isLoad) loadOp = NuLoadOpFor(loadOp);
    } break;
    default:
        ERROR(node, "Unhandled symbol type for %s", sym->user_name);
        return loadOp;
    }
    if (offsetValid) {
        NuEmitConst(irl, offset);
    }
    if (offsetOp != NU_OP_ILLEGAL) {
        ir = NuEmitOp(irl, offsetOp);
        ir->comment = auto_printf(128,"addr of %s", name);
    }
    return loadOp;
}

// figure out address of an array
static NuIrOpcode
NuCompileArrayAddress(NuIrList *irl, AST *node, int isLoad)
{
    NuIrOpcode op = NU_OP_ILLEGAL;
    AST *base = node->left;
    AST *index = node->right;
    AST *typ = ExprType(node);
    (void)NuCompileLhsAddress(irl, base); // op doesn't matter yet
    if (!IsConstZero(index)) {
        int siz = TypeSize(typ);
        if (siz > 1) {
            NuCompileMul(irl, index, AstInteger(siz), 0);
        } else {
            int n = NuCompileExpression(irl, index);
            if (n != 1) {
                ERROR(node, "Bad array indexing");
            }
        }
        NuEmitOp(irl, NU_OP_ADD);
    }
    op = LoadStoreOp(typ, isLoad);
    return op;
}

/* find opposite comparison for branch */
NuIrOpcode NuFlipCondition(NuIrOpcode op) {
    switch (op) {
    case NU_OP_BZ:    return NU_OP_BNZ;
    case NU_OP_BNZ:   return NU_OP_BZ;
    case NU_OP_BRA:   return NU_OP_DROP2;
    case NU_OP_DROP2: return NU_OP_BRA;
    case NU_OP_CBEQ:  return NU_OP_CBNE;
    case NU_OP_CBNE:  return NU_OP_CBEQ;
    case NU_OP_CBLTS: return NU_OP_CBGES;
    case NU_OP_CBGES: return NU_OP_CBLTS;
    case NU_OP_CBLES: return NU_OP_CBGTS;
    case NU_OP_CBGTS: return NU_OP_CBLES;
    case NU_OP_CBLTU: return NU_OP_CBGEU;
    case NU_OP_CBGEU: return NU_OP_CBLTU;
    case NU_OP_CBLEU: return NU_OP_CBGTU;
    case NU_OP_CBGTU: return NU_OP_CBLEU;
    default: ERROR(NULL, "Bad opcode to NuFlipCondition");
    }
    return NU_OP_ILLEGAL;
}

/* compile a boolean expression */
/* returns the opcode to use for the test */
static NuIrOpcode
NuCompileBasicBoolExpression(NuIrList *irl, AST *expr)
{
    NuIrOpcode opc = NU_OP_ILLEGAL;
    AST *left, *right;
    int n;
    
    int opkind = (expr->kind == AST_OPERATOR) ? expr->d.ival : -1;
    left = expr->left;
    right = expr->right;
    switch (opkind) {
    default:
        opc = NU_OP_CBNE; left = expr; right = AstInteger(0);
        break;
    case K_EQ: opc = NU_OP_CBEQ; break;
    case K_NE: opc = NU_OP_CBNE; break;
    case '<':  opc = NU_OP_CBLTS; break;
    case K_LE: opc = NU_OP_CBLES; break;
    case K_LTU: opc = NU_OP_CBLTU; break;
    case K_LEU: opc = NU_OP_CBLEU; break;
    case '>':   opc = NU_OP_CBGTS; break;
    case K_GE:  opc = NU_OP_CBGES; break;
    case K_GTU: opc = NU_OP_CBGTU; break;
    case K_GEU: opc = NU_OP_CBGEU; break;
    }
    n = NuCompileExpression(irl, left);
    if (n != 1) { ERROR(left, "Expected single value in boolean expression"); }
    n = NuCompileExpression(irl, right);
    if (n != 1) { ERROR(left, "Expected single value in boolean expression"); }
    return opc;
}

/* compile a boolean branch */
static void
NuCompileBoolBranches(NuIrList *irl, AST *expr, NuIrLabel *truedest, NuIrLabel *falsedest)
{
    NuIrLabel *dummylabel = NULL;
    int opkind;
    NuIrOpcode opc;
    
    if (IsConstExpr(expr)) {
        int x = EvalConstExpr(expr);
        if (x && truedest) NuEmitBranch(irl, NU_OP_BRA, truedest);
        if (!x && falsedest) NuEmitBranch(irl, NU_OP_BRA, falsedest);
    }
    if (expr->kind == AST_ISBETWEEN) {
        int n;
        bool needNewFalsedest = false;
        n = NuCompileExpression(irl, expr->right->left);
        if (n != 1) ERROR(expr, "Unexpected size of expression in ISBETWEEN");
        n = NuCompileExpression(irl, expr->right->right);
        if (n != 1) ERROR(expr, "Unexpected size of expression in ISBETWEEN");
        NuEmitOp(irl, NU_OP_DUP2);  // stack now looks like A B A B
        // make stack look like MIN(A, B) MAX(A, B)
        NuEmitOp(irl, NU_OP_MINS);  // A B MIN(A, B)
        NuEmitOp(irl, NU_OP_SWAP2);  // MIN(A, B) A B
        NuEmitOp(irl, NU_OP_MAXS);   // MIN(A, B) MAX(A, B)
        n = NuCompileExpression(irl, expr->left); // MIN MAX v
        if (n != 1) ERROR(expr, "Unexpected size of expression in ISBETWEEN");
        NuEmitOp(irl, NU_OP_DUP);    // MIN MAX v v
        NuEmitOp(irl, NU_OP_SWAP2);  // MIN v MAX v
        // want to verify x <= MAX and x >= MIN
        if (!falsedest) {
            falsedest = NuCreateLabel();
            needNewFalsedest = true;
        }
        NuEmitBranch(irl, NU_OP_CBGTS, falsedest);
        NuEmitBranch(irl, NU_OP_CBLTS, falsedest);
        if (truedest) {
            NuEmitBranch(irl, NU_OP_BRA, truedest);
        }
        if (needNewFalsedest) {
            NuEmitLabel(irl, falsedest);
        }
        return;
    }
    if (expr->kind == AST_OPERATOR) {
        opkind = expr->d.ival;
    } else {
        opkind = -1;
    }
    switch (opkind) {
    case K_BOOL_NOT:
        NuCompileBoolBranches(irl, expr->right, falsedest, truedest);
        break;
    case K_BOOL_AND:
        if (!falsedest) {
            falsedest = dummylabel = NuCreateLabel();
        }
        NuCompileBoolBranches(irl, expr->left, NULL, falsedest);
        NuCompileBoolBranches(irl, expr->right, truedest, falsedest);
        if (dummylabel) {
            NuEmitLabel(irl, dummylabel);
        }
        break;
    case K_BOOL_OR:
        if (!truedest) {
            truedest = dummylabel = NuCreateLabel();
        }
        NuCompileBoolBranches(irl, expr->left, truedest, NULL);
        NuCompileBoolBranches(irl, expr->right, truedest, falsedest);
        if (dummylabel) {
            NuEmitLabel(irl, dummylabel);
        }
        break;
    default:
        opc = NuCompileBasicBoolExpression(irl, expr);
        if (!truedest) {
            opc = NuFlipCondition(opc);
            truedest = falsedest;
            falsedest = NULL;
        }
        NuEmitBranch(irl, opc, truedest);
        if (falsedest && opc != NU_OP_BRA) {
            NuEmitBranch(irl, NU_OP_BRA, falsedest);
        }
        if (dummylabel) {
            NuEmitLabel(irl, dummylabel);
        }
        break;
    }       
}
/* compile address for lhs of assignment, return store op (or NU_OP_ILLEGAL if fail) */
static NuIrOpcode NuCompileLhsAddress(NuIrList *irl, AST *lhs)
{
    NuIrOpcode op = NU_OP_ILLEGAL;
    switch (lhs->kind) {
    case AST_SYMBOL:
    case AST_IDENTIFIER:
    case AST_LOCAL_IDENTIFIER:
        op = NuCompileIdentifierAddress(irl, lhs, 0);
        break;
    case AST_RESULT:
        if (curfunc->resultexpr && IsIdentifier(curfunc->resultexpr)) {
            return NuCompileLhsAddress(irl, curfunc->resultexpr);
        }
        if (LookupSymbolInTable(&curfunc->localsyms, "result")) {
            op = NuCompileIdentifierAddress(irl, AstIdentifier("result"), 0);
        } else {
            ERROR(lhs, "cannot handle RESULT keyword yet");
        }
        break;
    case AST_ARRAYREF:
        op = NuCompileArrayAddress(irl, lhs, 0);
        break;
    case AST_MEMREF: {
        int n = NuCompileExpression(irl, lhs->right);
        AST *typ = lhs->left;
        op = LoadStoreOp(typ, 0);
        if (n != 1) {
            ERROR(lhs, "too many values pushed on stack");
        }
    } break;
    case AST_METHODREF: {
        AST *objref = lhs->left;
        AST *methodname = lhs->right;
        AST *typ = ExprType(lhs->left);
        Module *P;
        Symbol *sym;
        if (!IsIdentifier(methodname)) {
            ERROR(lhs, "Expected identifier after .");
            return op;
        }
        const char *memberName = GetUserIdentifierName(methodname);
        if ( !IsClassType(typ) || NULL == (P=GetClassPtr(typ)) ) {
            ERROR(lhs, "Request for member %s in something not an object", memberName);
            return op;
        }
        sym = LookupSymbolInTable(&P->objsyms, memberName);
        if (!sym) {
            ERROR(lhs, "unknown symbol %s", memberName);
            return op;
        }
        switch(sym->kind) {
        case SYM_VARIABLE:
            (void)NuCompileLhsAddress(irl, objref);  // don't care about load op
            typ = sym->val;
            NuEmitConst(irl, sym->offset);
            NuEmitCommentedOp(irl, NU_OP_ADD, auto_printf(128, "lookup member %s", memberName));
            op = LoadStoreOp(typ, 0);
            break;
        case SYM_FUNCTION: {
            Function *F = (Function *)sym->val;
            NuPrepareFunctionBedata(F);
            NuEmitCommentedAddress(irl, FunData(F)->entryLabel, F->name);
            op = LoadStoreOp(ast_type_long, 0);
            return op;
        }
        case SYM_LABEL: {
            Label *lab = sym->val;
            NuIrLabel *nulabel = NULL;
            uint32_t labelval = lab->hubval;
            Module *Q = sym->module;
            if (!Q) {
                Q = current;
            }
            int offset = labelval;
            op = LoadStoreOp(lab->type, 0);
            if (!ModData(Q)) {
                NuPrepareModuleBedata(Q);
            }
            nulabel = ModData(Q)->datLabel;
            nulabel = NuIrOffsetLabel(nulabel, offset);
            NuEmitCommentedAddress(irl, nulabel, memberName);
            return op;
        }
        default:
            ERROR(lhs, "Wrong kind of symbol (%d) in method reference", sym->kind);
            break;
        }
    } break;
    case AST_HWREG: {
        HwReg *hwreg = (HwReg *)lhs->d.ptr;
        NuEmitConst(irl, hwreg->addr);
        op = NU_OP_STREG;
    } break;
    default:
        ERROR(lhs, "Address type not handled yet");
        break;
    }
    return op;
}

/* pop multiple things off the stack, in reverse order */
static int NuPopMultiple(NuIrList *irl, AST *lhs, int numRhs) {
    int popped = 0;
    NuIrOpcode op;
    int siz;
    if (!lhs) {
        if (numRhs > 0) {
            NuCompileDrop(irl, numRhs);
        }
        return 0;
    }
    if (lhs->kind == AST_EXPRLIST) {
        siz = (TypeSize(ExprType(lhs->left)) + 3) / 4;
        popped = NuPopMultiple(irl, lhs->right, numRhs-siz);
        if (lhs->left && lhs->left->kind == AST_EMPTY) {
            NuEmitOp(irl, NU_OP_DROP); // just drop the result
        } else {
            op = NuCompileLhsAddress(irl, lhs->left);
            if (op == NU_OP_ILLEGAL) {
                ERROR(lhs->left, "Do not know how to store into this yet");
            } else {
                NuEmitOp(irl, op);
            }
        }
        return popped+siz;
    }
    // hmmm, just one item; treat it like all other items were AST_EMPTY
    siz = (TypeSize(ExprType(lhs)) + 3) / 4;
    popped = numRhs - siz;
    if (popped > 0) {
        NuCompileDrop(irl, popped);
    }
    op = NuCompileLhsAddress(irl, lhs);
    if (op == NU_OP_ILLEGAL) {
        ERROR(lhs->left, "Do not know how to store into this yet");
    } else {
        NuEmitOp(irl, op);
    }
    return popped+siz;
}

// compile a masked fetch (a & mask) | val
// here a is expr->left, mask = expr->right->left, val = expr->right->right
// useful for manipulating groups of bits in OUTA and DIRA
static int NuCompileMaskMove(NuIrList *irl, AST *expr)
{
    AST *destast = expr->left;
    AST *maskast;
    AST *valast;
    
    if (expr->right->kind == AST_SEQUENCE) {
        maskast = expr->right->left;
        valast = expr->right->right;
    } else {
        ERROR(expr, "Internal format error");
        return 1;
    }
    // push A
    if (1 != NuCompileExpression(irl, destast)) ERROR(destast, "too many values in expression");
    if (1 != NuCompileExpression(irl, maskast)) ERROR(maskast, "too many values in expression");
    NuEmitOp(irl, NU_OP_AND);
    if (1 != NuCompileExpression(irl, valast))  ERROR(valast, "too many values in expression");
    NuEmitOp(irl, NU_OP_IOR);
    return 1;
}

/* compile assignment */
/* returns number of longs left on stack */
static int NuCompileAssign(NuIrList *irl, AST *ast, int inExpression)
{
    AST *lhs = ast->left;
    AST *rhs = ast->right;
    int n;
    NuIrOpcode op;

    if (ast->d.ival != K_ASSIGN) {
        ERROR(ast, "Cannot handle complex assignment");
    }
    n = NuCompileExpression(irl, rhs);
    if (n == 0) {
        ERROR(rhs, "Assignment from void value");
        return 1;
    } else if (n != 1 && inExpression) {
        // we will leave nothing on the stack in this case
        inExpression = 0;
    }
    if (inExpression) {
        NuEmitOp(irl, NU_OP_DUP); // leave last value on stack
    }
    if (n > 1) {
        // handle multiple assignments
        // if insufficient items given, just drop the rest
        int got = NuPopMultiple(irl, lhs, n);
        if (got != n) {
            ERROR(rhs, "Expected to assign %d items but only did %d", n, got);
        }
        return 0; // everything has been popped
    }
    // single assignment
    op = NuCompileLhsAddress(irl, lhs);
    if (op == NU_OP_ILLEGAL) {
        ERROR(lhs, "Do not know how to store into this yet");
    } else {
        NuEmitOp(irl, op);
    }
    return inExpression;
}

static int
NuCompileMul(NuIrList *irl, AST *lhs, AST *rhs, int gethi)
{
    int isUnsigned = (gethi & 2);
    int needHi = (gethi & 1);
    AST *tmp;

    // optimize some constant multiplies
    // make sure constant is on the right
    if (IsConstExpr(lhs)) {
        if (IsConstExpr(rhs)) {
            NuEmitConst(irl, EvalConstExpr(lhs) * EvalConstExpr(rhs));
            return 1;
        }
        tmp = lhs; lhs = rhs; rhs = tmp;
    }
    NuCompileExpression(irl, lhs);
    if (IsConstExpr(rhs)) {
        // do some simple optimizations
        int r = EvalConstExpr(rhs);
        switch (r) {
        case 1:
            return 1; // nothing to do
        case -1:
            NuEmitOp(irl, NU_OP_NEG);
            return 1;
        case 2:
            NuEmitOp(irl, NU_OP_DUP);
            NuEmitOp(irl, NU_OP_ADD);
            return 1;
        case 3:
            NuEmitOp(irl, NU_OP_DUP);
            NuEmitOp(irl, NU_OP_DUP);
            NuEmitOp(irl, NU_OP_ADD);
            NuEmitOp(irl, NU_OP_ADD);
            return 1;
        case 4:
            NuEmitConst(irl, 2);
            NuEmitOp(irl, NU_OP_SHL);
            return 1;
        case 8:
            NuEmitConst(irl, 3);
            NuEmitOp(irl, NU_OP_SHL);
            return 1;
        case 16:
            NuEmitConst(irl, 4);
            NuEmitOp(irl, NU_OP_SHL);
            return 1;
        default:
            break;
        }
    }
    NuCompileExpression(irl, rhs);
    if (isUnsigned) {
        NuEmitOp(irl, NU_OP_MULU);
    } else {
        NuEmitOp(irl, NU_OP_MULS);
    }
    // lo, hi are placed on stack
    if (needHi) {
        NuEmitOp(irl, NU_OP_SWAP);
    }
    NuEmitOp(irl, NU_OP_DROP);
    return 1;
}

static int
NuCompileDiv(NuIrList *irl, AST *lhs, AST *rhs, int gethi)
{
    int isUnsigned = (gethi & 2);
    int needHi = (gethi & 1);

    // FIXME: should optimize here for shifts and such
    NuCompileExpression(irl, lhs);
    NuCompileExpression(irl, rhs);
    if (isUnsigned) {
        NuEmitOp(irl, NU_OP_DIVU);
    } else {
        NuEmitOp(irl, NU_OP_DIVS);
    }
    // lo, hi are placed on stack
    if (needHi) {
        NuEmitOp(irl, NU_OP_SWAP);
    }
    NuEmitOp(irl, NU_OP_DROP);
    return 1;
}

/* returns number of longs pushed on stack */
static int
NuCompileOperator(NuIrList *irl, AST *node) {
    AST *lhs, *rhs;
    int optoken;
    int pushed = 0;
    int isBool;
    
    lhs = node->left;
    rhs = node->right;
    optoken = node->d.ival;

    switch (optoken) {
    case K_BOOL_NOT:
    case K_BOOL_AND:
    case K_BOOL_OR:
    case K_EQ:
    case K_NE:
    case K_LE:
    case K_GE:
    case '<':
    case '>':
    case K_LTU:
    case K_GTU:
    case K_LEU:
    case K_GEU:
        isBool = 1;
        break;            
    default: isBool = 0; break;
    }
    if (isBool) {
        NuIrLabel *skiplabel = NuCreateLabel();
        int truevalue = LangBoolIsOne(curfunc->language) ? 1 : -1;

        pushed = 1;
        NuEmitConst(irl, 0);
        NuCompileBoolBranches(irl, node, NULL, skiplabel);
        NuEmitConst(irl, truevalue);
        NuEmitOp(irl, NU_OP_XOR);
        NuEmitLabel(irl, skiplabel);
    } else if (optoken == K_INCREMENT || optoken == K_DECREMENT) {
        // handle some special cases here
        AST *dest = (node->left) ? node->left : node->right;
        AST *desttype = ExprType(dest);
        NuIrOpcode math = (optoken == K_INCREMENT) ? NU_OP_ADD : NU_OP_SUB;
        NuIrOpcode ldOp, stOp;
        int stepSize = 1;
        int postIncDec = node->left != 0;
        
        if (IsPointerType(desttype)) {
            stepSize = TypeSize(BaseType(desttype));
        } else if (IsFloatType(desttype)) {
            ERROR(node, "Cannot handle ++ or -- for floats yet");
        }
        pushed = 1;
        stOp = NuCompileLhsAddress(irl, dest);
        if (stOp == NU_OP_ILLEGAL) return 1;
        ldOp = NuLoadOpFor(stOp);
        NuEmitOp(irl, ldOp);
        if (postIncDec) {
            NuEmitOp(irl, NU_OP_DUP);
        }
        NuEmitConst(irl, stepSize);
        NuEmitOp(irl, math);
        if (!postIncDec) {
            NuEmitOp(irl, NU_OP_DUP);
        }
        NuCompileLhsAddress(irl, dest);
        NuEmitOp(irl, stOp);
        pushed = 1;
    } else if (optoken == '*') {            pushed = NuCompileMul(irl, node->left, node->right, 0);
    } else if (optoken == K_HIGHMULT) {     pushed = NuCompileMul(irl, node->left, node->right, 1);
    } else if (optoken == K_UNS_HIGHMULT) { pushed = NuCompileMul(irl, node->left, node->right, 3);
    } else if (optoken == '/') {            pushed = NuCompileDiv(irl, node->left, node->right, 0);
    } else if (optoken == K_MODULUS) {      pushed = NuCompileDiv(irl, node->left, node->right, 1);
    } else if (optoken == K_UNS_DIV) {      pushed = NuCompileDiv(irl, node->left, node->right, 2);
    } else if (optoken == K_UNS_MOD) {      pushed = NuCompileDiv(irl, node->left, node->right, 3);
    } else if (optoken == K_REV) {
        NuCompileExpression(irl, lhs);
        NuCompileExpression(irl, rhs);
        NuEmitOp(irl, NU_OP_REV);
        NuEmitOp(irl, NU_OP_SHR);
        pushed = 1;
    } else if (optoken == K_SIGNEXTEND || optoken == K_ZEROEXTEND) {
        AST *rhs_temp = AstOperator('-', rhs, AstInteger(1));
        rhs_temp = FoldIfConst(rhs_temp);
        if (NuCompileExpression(irl, lhs) != 1) { ERROR(lhs, "too many values"); };
        NuCompileExpression(irl, rhs_temp);
        NuEmitOp(irl, (optoken == K_SIGNEXTEND) ? NU_OP_SIGNX : NU_OP_ZEROX);
        pushed = 1;
    } else {
        if (lhs) {
            pushed += NuCompileExpression(irl, lhs);
        }
        if (rhs) {
            pushed += NuCompileExpression(irl, rhs);
        }
        // could sanity check pushed here
        // assume we will push just one item
        pushed = 1;
        switch (optoken) {
        case K_NEGATE: NuEmitOp(irl, NU_OP_NEG); break;
        case K_BIT_NOT: NuEmitOp(irl, NU_OP_NOT); break;
        case K_ABS: NuEmitOp(irl, NU_OP_ABS); break;
        case '+': NuEmitOp(irl, NU_OP_ADD); break;
        case '-': NuEmitOp(irl, NU_OP_SUB); break;
        case '&': NuEmitOp(irl, NU_OP_AND); break;
        case '|': NuEmitOp(irl, NU_OP_IOR); break;
        case '^': NuEmitOp(irl, NU_OP_XOR); break;
        case K_SHL: NuEmitOp(irl, NU_OP_SHL); break;
        case K_SHR: NuEmitOp(irl, NU_OP_SHR); break;
        case K_SAR: NuEmitOp(irl, NU_OP_SAR); break;
        case K_ROTL: NuEmitOp(irl, NU_OP_ROL); break;
        case K_ROTR: NuEmitOp(irl, NU_OP_ROR); break;
        case K_ENCODE: NuEmitOp(irl, NU_OP_ENCODE); break;
        case K_ENCODE2: NuEmitOp(irl, NU_OP_ENCODE2); break;
        case K_LIMITMIN: NuEmitOp(irl, NU_OP_MINS); break;
        case K_LIMITMAX: NuEmitOp(irl, NU_OP_MAXS); break;
        case K_LIMITMIN_UNS: NuEmitOp(irl, NU_OP_MINU); break;
        case K_LIMITMAX_UNS: NuEmitOp(irl, NU_OP_MAXU); break;
        default:
            ERROR(node, "Unable to handle operator 0x%x", optoken);
            break;
        }
    }
    return pushed;
}

/* returns number of longs pushed on stack */
static int NuCompileCondResult(NuIrList *irl, AST *expr) {
    AST *cond = expr->left;
    AST *ifpart = expr->right->left;
    AST *elsepart = expr->right->right;
    NuIrLabel *label1 = NuCreateLabel();
    NuIrLabel *label2 = NuCreateLabel();
    int n_if, n_else;
    
    NuCompileBoolBranches(irl, cond, NULL, label1);

    /* the default is the IF part */
    n_if = NuCompileExpression(irl, ifpart);
    NuEmitBranch(irl, NU_OP_BRA, label2);

    /* here is the else part */
    NuEmitLabel(irl, label1);
    n_else = NuCompileExpression(irl, elsepart);
    NuEmitLabel(irl, label2);

    if (n_if != n_else) {
        ERROR(expr, "different number of results in ?: sides");
    }
    
    return n_if;
}

//
// compile val += n
//
static void
NuCompileIncrement(NuIrList *irl, AST *val, int n) {
    NuIrOpcode stOp;
    
    if (n == 0) return;
    NuCompileExpression(irl, val);
    NuEmitConst(irl, n);
    NuEmitCommentedOp(irl, NU_OP_ADD, "increment pointer");
    stOp = NuCompileLhsAddress(irl, val);
    NuEmitOp(irl, stOp);
}

//
// compile a coginit expression
// if we have a spin function call inside it then this
// is mildly complicated
//
#define NU_INTERP_START 0x80

static int
NuCompileCoginit(NuIrList *irl, AST *expr)
{
    AST *funccall;
    AST *params = expr->left;
    int n;
    
    if ( IsSpinCoginit(expr, NULL) ) {
        AST *exprlist;
        AST *funccall;
        AST *objref = 0;
        AST *stack;
        AST *cogid;
        AST *tmpreg = AstIdentifier("__interp_temp1");
        NuIrOpcode stOp;
        Symbol *sym;
        
        // need to push some stuff onto the new stack, namely:
        //   the arguments
        //   initial PC
        //   initial object base
        exprlist = expr->left;
        if (!exprlist) {
            ERROR(expr, "Missing cog parameter for coginit/cognew"); return 1;
        }
        cogid = exprlist->left;
        exprlist = exprlist->right;
        if (!exprlist) {
            ERROR(expr, "Missing function parameter for coginit/cognew"); return 1;
        }
        funccall = exprlist->left;
        exprlist = exprlist->right;
        if (!exprlist) {
            ERROR(expr, "Missing stack parameter for coginit/cognew"); return 1;
        }
        stack = exprlist->left;
        if (exprlist->right != NULL) {
            ERROR(expr, "Too many parameters to coginit/cognew");
        }
        // get initial stack pointer into tmpreg
        n = NuCompileExpression(irl, stack);
        if (n != 1) { ERROR(expr, "Bad stack value"); return 1; }
        stOp = NuCompileLhsAddress(irl, tmpreg);
        NuEmitCommentedOp(irl, stOp, "get copy of stack value");
        // compile arguments
        params = funccall->right;
        // make sure we push at least 2 items onto the stack (2 will be popped by the caller)
        n = AstListLen(params);
        while (n < 2) {
            params = NewAST(AST_EXPRLIST, AstInteger(0), params);
            ++n;
        }
        while (params) {
            AST *arg = params->left;
            params = params->right;
            n = NuCompileExpression(irl, arg);  // n is number of longs pushed
            NuCompileExpression(irl, tmpreg);
            switch (n) {
            case 1: NuEmitCommentedOp(irl, NU_OP_STL, "pop long arg"); break;
            case 2: NuEmitCommentedOp(irl, NU_OP_STD, "pop double arg"); break;
            default:
                ERROR(arg, "argument too long for coginit"); return 1;
            }
            // increment tmpreg pointer
            NuCompileIncrement(irl, tmpreg, n*LONG_SIZE);
        }
        // now compile initial PC
        sym = FindFuncSymbol(funccall, &objref, 1);
        if (!sym || sym->kind != SYM_FUNCTION) {
            ERROR(funccall, "Not a regular function, bailing");
            return 1;
        }
        Function *F = (Function *)sym->val;
        if (F->body->kind == AST_BYTECODE) {
            ERROR(funccall, "Internal error, function is actually a single bytecode op");
            return 1;
        }
        // push new PC
        NuPrepareFunctionBedata(F);
        NuEmitCommentedAddress(irl, FunData(F)->entryLabel, F->name);
        // copy to final stack
        NuCompileExpression(irl, tmpreg);
        NuEmitCommentedOp(irl, NU_OP_STL, "save pc");
        NuCompileIncrement(irl, tmpreg, LONG_SIZE);
        
        // push new VBASE
        if (!objref) {
            NuEmitConst(irl, 0);
            NuEmitCommentedOp(irl, NU_OP_ADD_VBASE, "push self");
        } else {
            NuCompileExpression(irl, objref);
        }
        // copy to final stack
        NuCompileExpression(irl, tmpreg);
        NuEmitCommentedOp(irl, NU_OP_STL, "save vbase");
        NuCompileIncrement(irl, tmpreg, LONG_SIZE);

        // OK, new stack is all set up
        // now we need to call coginit(cogid, @entry, stack)
        params = NewAST( AST_EXPRLIST, cogid,
                         NewAST(AST_EXPRLIST, AstInteger(NU_INTERP_START),
                                NewAST(AST_EXPRLIST, tmpreg, NULL)) );

    }
    funccall = AstIdentifier("_coginit");
    funccall = NewAST(AST_FUNCCALL, funccall, params);
    return NuCompileFunCall(irl, funccall);
}

//
// compile a lookup/lookdown
//
static int
NuCompileLookupDown(NuIrList *irl, AST *expr)
{
    AST *funccall;
    AST *idx, *base;
    AST *params;
    AST *ev, *table;
    AST *len;
    AST *arrid;
    AST *tmpreg1 = NULL;
    AST *tmpreg2 = NULL;
    int pushed;
    int popsize = 0;
    if (expr->kind == AST_LOOKDOWN) {
        funccall = AstIdentifier("_lookdown");
    } else {
        funccall = AstIdentifier("_lookup");
    }
    ev = expr->left;
    if (ev->kind != AST_LOOKEXPR) {
        ERROR(ev, "Internal error in lookup");
        return 1;
    }
    base = ev->left;
    idx = ev->right;
    table = expr->right;

    if (table->kind == AST_TEMPARRAYUSE) {
        len = table->right;
        arrid = NewAST(AST_ADDROF, table->left, NULL);
    } else if (table->kind == AST_EXPRLIST) {
        // FIXME?? ASSUMES STACK GROWS UP!!!
        tmpreg1 = AstIdentifier("__interp_temp1");
        tmpreg2 = AstIdentifier("__interp_temp2");
        
        /* NOTE!
           we have to evaluate the index before evaluating array elements
        */
        /* fixme: will nested lookup work?? */
        pushed = NuCompileExpression(irl, idx);
        NuCompileLhsAddress(irl, tmpreg1);
        NuEmitOp(irl, NU_OP_STREG);

        /* get the stack pointer */
        NuEmitConst(irl, 0);
        NuEmitOp(irl, NU_OP_ADD_SP);
        NuCompileLhsAddress(irl, tmpreg2);
        NuEmitOp(irl, NU_OP_STREG);
        
        idx = tmpreg1;
        arrid = tmpreg2;
        
        // push the arguments onto the stack
        popsize = NuCompileExpression(irl, table);
        len = AstInteger(popsize);
    } else {
        ERROR(table, "Internal error in lookup code");
        return 1;
    }

    params = NewAST(AST_EXPRLIST, idx,
                    NewAST(AST_EXPRLIST, base,
                           NewAST(AST_EXPRLIST, arrid,
                                  NewAST(AST_EXPRLIST, len, NULL))));
    funccall = NewAST(AST_FUNCCALL, funccall, params);
    pushed = NuCompileFunCall(irl, funccall);
    if (pushed != 1) {
        ERROR(table, "Internal error, pushed too many results for lookup/down");
    }
    if (popsize) {
        NuCompileLhsAddress(irl, tmpreg1);
        NuEmitOp(irl, NU_OP_STREG);  // save final value
        NuCompileDrop(irl, popsize);         // drop the lookup/down
        NuCompileLhsAddress(irl, tmpreg1);
        NuEmitOp(irl, NU_OP_LDREG);
    }
    return pushed;
}

/* compile alloca(N) */
static void
NuCompileAlloca(NuIrList *irl, AST *siz) {
    int n;
    NuEmitConst(irl, 0);
    NuEmitCommentedOp(irl, NU_OP_ADD_SP, "alloca base");
    n = NuCompileExpression(irl, siz);
    if (n != 1) ERROR(siz, "too many values passed to alloca");
    NuEmitCommentedOp(irl, NU_OP_ADD_SP, "allocate stack space");
    NuCompileLhsAddress(irl, nu_stack_ptr);
    NuEmitCommentedOp(irl, NU_OP_STREG, "update stack pointer"); 
}

/* compile freea(ptr): ptr is already on stack */
static void
NuCompileFreea(NuIrList *irl) {
    NuEmitOp(irl, NU_OP_SET_SP);
}

/* returns number of longs pushed on stack */
static int
NuCompileExpression(NuIrList *irl, AST *node) {
    int pushed = 0; // number results pushed on stack
    if(!node) {
        // nothing to do here
        return pushed;
    }
    if (IsConstExpr(node)) {
        int64_t val = EvalConstExpr(node);
        int siz = TypeSize(ExprType(node));
        if (siz <= 4) {
            NuEmitConst(irl, (int32_t)val);
            return 1;
        } else {
            siz = (siz + 3) / 4;
            for (int i = 0; i < siz; i++) {
                NuEmitConst(irl, (int32_t)val);
                val = val >> 32;
            }
            return siz;
        }
    }
    switch(node->kind) {
    case AST_SYMBOL:
    case AST_IDENTIFIER:
    case AST_LOCAL_IDENTIFIER:
    case AST_ARRAYREF:
    case AST_METHODREF:
    case AST_MEMREF:
    case AST_POSTSET:
    {
        AST *typ;
        int siz;
        NuIrOpcode storeOp, loadOp;
        bool isPostSet = (node->kind == AST_POSTSET);
        AST *ref = isPostSet ? node->left : node;
        
        typ = ExprType(ref);
        if (!typ) {
            typ = ast_type_long;
        }
        siz = TypeSize(typ);
        storeOp = NuCompileLhsAddress(irl, ref);
        pushed = (siz+3)/4;
        if (isPostSet) {
            if (pushed != 1) {
                ERROR(node, "\\ operator only valid for single values");
                pushed = 1;
            }
            NuEmitOp(irl, NU_OP_DUP);  // stack looks like A A
        }
        if (storeOp == NU_OP_ILLEGAL) {
            ERROR(node, "Unable to evaluate expression");
        } else {
            loadOp = NuLoadOpFor(storeOp);
            NuEmitOp(irl, loadOp);
            if (isPostSet) {
                NuEmitOp(irl, NU_OP_SWAP); // stack looks like origV A
                pushed = NuCompileExpression(irl, node->right); // origV A newV
                if (pushed != 1) {
                    ERROR(node, "\\ requires a single value on right hand side");
                    pushed = 1;
                }
                NuEmitOp(irl, NU_OP_SWAP); // stack looks like origV newV A
                NuEmitOp(irl, storeOp);
            }
        }
    } break;
    case AST_FUNCCALL:
    {
        pushed = NuCompileFunCall(irl, node);
    } break;
    case AST_OPERATOR:
    {
        pushed = NuCompileOperator(irl, node);
    } break;
    case AST_MASKMOVE:
    {
        pushed = NuCompileMaskMove(irl, node);
    } break;
    case AST_ABSADDROF:
    case AST_ADDROF:
    {
        (void)NuCompileLhsAddress(irl, node->left); // don't care about load op, we will not use it
        pushed = 1;
    } break;
    case AST_DATADDROF:
    {
        pushed = NuCompileExpression(irl, node->left); // don't care about load op, we will not use it
        NuEmitCommentedAddress(irl, ModData(current)->datLabel, "DAT address");
        NuEmitCommentedOp(irl, NU_OP_ADD, "compute @@");
    } break;
    case AST_STRINGPTR:
    {
        NuIrLabel *tmpLabel;
        NuFunData *fdata = FunData(curfunc);
        
        if (!fdata->dataLabel) {
            fdata->dataLabel = NuCreateLabel();
            flexbuf_init(&fdata->dataBuf, 256);
        }
        tmpLabel = NuIrOffsetLabel(fdata->dataLabel, flexbuf_curlen(&fdata->dataBuf));
        StringBuildBuffer(&fdata->dataBuf, node->left);
        NuEmitAddress(irl, tmpLabel);
        pushed = 1;
    } break;
    case AST_CAST:
        return NuCompileExpression(irl, node->right);
    case AST_DECLARE_VAR:
        return NuCompileExpression(irl, node->right);
    case AST_SEQUENCE: {
        int n = NuCompileExpression(irl, node->left);
        NuCompileDrop(irl, n);
        return NuCompileExpression(irl, node->right);
    } break;
    case AST_GETLOW:
    case AST_GETHIGH: {
        int n = NuCompileExpression(irl, node->left);
        if (n != 2) {
            WARNING(node, "Expected 64 bit value");
        }
        if (node->kind == AST_GETHIGH) {
            NuEmitCommentedOp(irl, NU_OP_SWAP, "get high word");
        }
        NuCompileDrop(irl, n-1);
        pushed = 1;
    } break;
    case AST_ASSIGN:
        pushed = NuCompileAssign(irl, node, 1);
        break;
    case AST_CONDRESULT:
        pushed = NuCompileCondResult(irl, node);
        break;
    case AST_HWREG: {
        HwReg *hw = (HwReg *)node->d.ptr;
        NuEmitConst(irl, hw->addr);
        NuEmitOp(irl, NU_OP_LDREG);
        pushed = 1;
    } break;
    case AST_EXPRLIST: {
        pushed = NuCompileExprList(irl, node);
    } break;
    case AST_RESULT: {
        if (!curfunc->resultexpr) {
            pushed = 1; NuEmitConst(irl, 0);
        } else if (curfunc->resultexpr->kind != AST_RESULT) {
            pushed = NuCompileExpression(irl, curfunc->resultexpr);
        } else {
            NuCompileLhsAddress(irl, node);
            NuEmitOp(irl, NU_OP_LDL);
            pushed = 1;
        }
    } break;
    case AST_SELF: {
        NuEmitConst(irl, 0);
        NuEmitCommentedOp(irl, NU_OP_ADD_VBASE, "self");
        pushed = 1;
    } break;
    case AST_LOOKUP:
    case AST_LOOKDOWN:
        pushed = NuCompileLookupDown(irl, node);
        break;
    case AST_COGINIT:
        pushed = NuCompileCoginit(irl, node);
        break;
    case AST_CATCH: {
        // rhs is a function call
        // compile to something like:
        // if (setjmp(tmpbuf) != 0) goto retjmp; func();
        NuIrLabel *retJmpVal = NuCreateLabel();
        NuCompileAlloca(irl, AstInteger(NU_JMP_BUF_SIZ));
        NuEmitCommentedOp(irl, NU_OP_DUP, "save alloca base");
        NuEmitCommentedOp(irl, NU_OP_SETJMP, "setjmp");
        NuEmitConst(irl, 0);
        NuEmitBranch(irl, NU_OP_CBNE, retJmpVal);
        // save initial setjmp return value (the jump buffer) into abortchain
        NuEmitOp(irl, NU_OP_DUP);
        NuCompileLhsAddress(irl, nu_abortchain_ptr);
        NuEmitCommentedOp(irl, NU_OP_STREG, "save into abortchain");
        pushed = NuCompileExpression(irl, node->left);
        if (pushed == 0) {
            NuEmitConst(irl, 0);
            pushed = 1;
        }
        if (pushed != 1) {
            ERROR(node, "too many return values from \\ protected call");
        }
        NuEmitLabel(irl, retJmpVal);
        // stack now has ALLOCA value and func return value, in that order
        NuEmitCommentedOp(irl, NU_OP_SWAP, "get alloca base on top");
        NuCompileFreea(irl);
        break;
    } break;
    case AST_TRYENV: {
        //  allocate space on stack
        ERROR(node, "Cannot handle tryenv in expressions yet");
        pushed = 1;
    } break;
    case AST_SETJMP: {
        int needpop = 0;
        if (node->left) {
            needpop = 1;
            ERROR(node, "cannot handle this setjmp in expressions yet");
        } else {
            NuCompileLhsAddress(irl, nu_abortchain_ptr);
            NuEmitCommentedOp(irl, NU_OP_LDREG, "use existing abort chain");
        }
        NuEmitCommentedOp(irl, NU_OP_SETJMP, "set jump buffer");
        NuEmitCommentedOp(irl, NU_OP_SWAP, "put abort result on top");
        if (needpop) {
            NuEmitCommentedOp(irl, NU_OP_DROP, "ignore second value");
        } else {
            NuCompileLhsAddress(irl, nu_abortresult_ptr);
            NuEmitCommentedOp(irl, NU_OP_STREG, "save abort result");
        }
        pushed = 1;
    } break;
    case AST_CATCHRESULT: {
        NuCompileLhsAddress(irl, nu_abortresult_ptr);
        NuEmitCommentedOp(irl, NU_OP_LDREG, "load last caught value");
        pushed = 1;
    } break;
    case AST_ALLOCA: {
        NuCompileAlloca(irl, node->right);
        pushed = 1;
    } break;
    case AST_CONSTANT: {
      if (!IsConstExpr(node->left)) {
          WARNING(node, "CONSTANT expression is not actually constant and will be evaluated at run time");
      }
      pushed = NuCompileExpression(irl, node->left);
    } break;
#if 0        
    case AST_VA_ARG: {
        AST *typ = ExprType(node->left);
        int siz = TypeSize(typ);
        AST *args = node->right;
        AST *incr;
        AST *fetch;
        
        if (siz > 8) {
            ERROR(node, "large varargs not supported in nucode");
            siz = 8;
        }
        fetch = NewAST(AST_MEMREF, typ, args);
        pushed = NuCompileExpression(irl, fetch);
        incr = AstAssign(args, AstOperator('+', args, AstInteger(siz)));
        NuCompileAssign(irl, incr, 0);
    } break;
#endif        
    default:
        ERROR(node, "Unknown expression node %d", node->kind);
        return 1;
    }
    return pushed;
}

static int
NuCompileExprList(NuIrList *irl, AST *node)
{
    int pushed = 0;
    while (node && node->kind == AST_EXPRLIST) {
        pushed += NuCompileExpression(irl, node->left);
        node = node->right;
    }
    if (node) {
        ERROR(node, "Expecting expression list");
    }
    return pushed;
}

static void
NuCompileStmtlist(NuIrList *irl, AST *list) {
    for (AST *ast=list; ast; ast=ast->right) {
        AST *node = ast->left;
        if (!node) {
            //ERROR(ast,"empty node?!?"); ignore, this can happen
            continue;
        }
        NuCompileStatement(irl,node);
    }
}

static void NuCompileInlineAsm(NuIrList *irl, AST *ast) {
    AST *list = ast->left;
    NuIrLabel *startLabel;
    NuFunData *fdata = FunData(curfunc);
    unsigned startPos, endPos;
    
    if (!fdata->dataLabel) {
        fdata->dataLabel = NuCreateLabel();
        flexbuf_init(&fdata->dataBuf, 1024);
    }
    startPos = flexbuf_curlen(&fdata->dataBuf);
    startLabel = NuIrOffsetLabel(fdata->dataLabel, startPos);
    AssignAddresses(&curfunc->localsyms, list, 0);
    PrintDataBlock(&fdata->dataBuf,list,NULL,NULL);
    // append a RET instruction
    // which is $FD64002D
    flexbuf_addchar(&fdata->dataBuf, 0x2d);
    flexbuf_addchar(&fdata->dataBuf, 0x00);
    flexbuf_addchar(&fdata->dataBuf, 0x64);
    flexbuf_addchar(&fdata->dataBuf, 0xfd);
    endPos = flexbuf_curlen(&fdata->dataBuf);

    // adjust to number of longs
    endPos = ((endPos - startPos) + 3) / 4;
    // sanity check size
    if (endPos > 0xff) {
        ERROR(ast, "Inline assembly is too long (%d longs)\n", endPos);
    }
    NuEmitAddress(irl, startLabel);
    NuEmitConst(irl, endPos - 1);
    NuEmitOp(irl, NU_OP_INLINEASM);
}

static void NuCompileForLoop(NuIrList *irl, AST *ast, int atleastonce) {
    AST *initstmt;
    AST *loopcond;
    AST *update;
    AST *body = 0;
    NuIrLabel *toplabel, *nextlabel, *exitlabel;
    
    initstmt = ast->left;
    ast = ast->right;
    if (!ast || ast->kind != AST_TO) {
        ERROR(ast, "Internal Error: expected AST_TO in for loop");
        return;
    }
    loopcond = ast->left;
    ast = ast->right;
    if (!ast || ast->kind != AST_STEP) {
        ERROR(ast, "Internal Error: expected AST_TO in for loop");
        return;
    }
    update = ast->left;
    ast = ast->right;
    body = ast;

    NuCompileStatement(irl, initstmt);

    toplabel = NuCreateLabel();
    nextlabel = NuCreateLabel();
    exitlabel = NuCreateLabel();
    NuPushQuitNext(exitlabel, nextlabel);
    if (!loopcond) {
        loopcond = AstInteger(1);
    }
    NuEmitLabel(irl, toplabel);
    if (!atleastonce) {
        NuCompileBoolBranches(irl, loopcond, NULL, exitlabel);
    }
    NuCompileStatement(irl, body);
    NuEmitLabel(irl, nextlabel);
    NuCompileStatement(irl, update);
    if (atleastonce) {
        NuCompileBoolBranches(irl, loopcond, toplabel, NULL);
    } else {
        NuEmitBranch(irl, NU_OP_BRA, toplabel);
    }
    NuEmitLabel(irl, exitlabel);
    NuPopQuitNext();   
}

static int NuDebugEval(AST *ast, int regNum, int *addr, void *ourArg) {
    NuIrList *irl = (NuIrList *)ourArg;
    int n;
    if (IsConstExpr(ast)) {
        *addr = EvalConstExpr(ast);
        return PASM_EVAL_ISCONST;
    }
    n = NuCompileExpression(irl, ast);
    if (n != 1) {
        ERROR(ast, "DEBUG expression has %d return values", n);
    }
    NuEmitConst(irl, regNum);
    NuEmitOp(irl, NU_OP_STREG);
    *addr = regNum;
    return PASM_EVAL_ISREG;
}

static void NuCompileStatement(NuIrList *irl, AST *ast) {
    int n;
    NuIrLabel *toploop, *botloop, *exitloop;

    while (ast && ast->kind == AST_COMMENTEDNODE) {
        ast = ast->left;
    }
    if (!ast) return;
    switch(ast->kind) {
    case AST_COMMENT:
        // for now, do nothing
        break;
    case AST_STMTLIST:
        NuCompileStmtlist(irl, ast);
        break;
    case AST_SCOPE: {
        // TODO: When we implement __builtin_alloca(), we should restore the
        // stack after compiling the contents of the scope.
        NuCompileStmtlist(irl, ast->left);
    } break;
    case AST_ASSIGN:
        NuCompileAssign(irl, ast, 0);
        break;
    case AST_GOSUB: {
        NuIrLabel *lab = NuGetLabelFromSymbol( ast, GetUserIdentifierName(ast->left) );
        if (curfunc->numparams != 0 || curfunc->numresults != 0) {
            WARNING(ast, "GOSUB in a function with parameters or results will probably not work correctly");
        }
        if (lab) {
            NuEmitConst(irl, NumLocalLongs(curfunc));
            NuEmitAddress(irl, lab);
            NuEmitOp(irl, NU_OP_GOSUB);
        }
    } break;
    case AST_GOTO: {
        NuIrLabel *lab = NuGetLabelFromSymbol( ast, GetUserIdentifierName(ast->left) );
        if (lab) {
            NuEmitBranch(irl, NU_OP_BRA, lab);
        }
    } break;
    case AST_LABEL: {
        NuIrLabel *lab = NuGetLabelFromSymbol( ast, GetUserIdentifierName(ast->left) );
        if (lab) {
            NuEmitLabel(irl, lab);
        }
    } break;
    case AST_JUMPTABLE: {
        NuIrLabel *jumptab = NuCreateLabel();
        NuIrLabel *op;
        // put selector on stack
        NuCompileExpression(irl, ast->left);
        NuEmitOp(irl, NU_OP_JMPREL);
        NuEmitLabel(irl, jumptab);
        ast = ast->right;
        while (ast && ast->kind == AST_LISTHOLDER) {
            op = NuGetLabelFromSymbol( ast, GetUserIdentifierName(ast->left) );
            if (op) { NuEmitBranch(irl, NU_OP_BRA, op); }
            ast = ast->right;
        }
        if (!ast || ast->kind != AST_STMTLIST) {
            ERROR(ast, "Expected statement list!"); break;
        }
        NuCompileStmtlist(irl, ast);
    } break;
    case AST_IF: {
        NuIrLabel *elselbl = NuCreateLabel();
        NuIrLabel *bottomlbl;
        AST *thenelse = ast->right;
        if (thenelse && thenelse->kind == AST_COMMENTEDNODE) thenelse = thenelse->left;
        ASSERT_AST_KIND(thenelse,AST_THENELSE,break;);
        NuCompileBoolBranches(irl, ast->left, NULL, elselbl);
        NuCompileStmtlist(irl, thenelse->left);
        if (thenelse->right) {
            bottomlbl = NuCreateLabel();
            NuEmitBranch(irl, NU_OP_BRA, bottomlbl);
            NuEmitLabel(irl, elselbl);
            NuCompileStmtlist(irl, thenelse->right);
        } else {
            bottomlbl = elselbl;
        }
        NuEmitLabel(irl, bottomlbl);
    } break;
        
    case AST_WHILE:
        toploop = NuCreateLabel();
        botloop = NuCreateLabel();
        NuPushQuitNext(botloop, toploop);
        NuEmitLabel(irl, toploop);
        NuCompileBoolBranches(irl, ast->left, NULL, botloop);
        NuCompileStmtlist(irl, ast->right);
        NuEmitBranch(irl, NU_OP_BRA, toploop);
        NuEmitLabel(irl, botloop);
        NuPopQuitNext();
        break;
    case AST_DOWHILE:
        toploop = NuCreateLabel();
	botloop = NuCreateLabel();
	exitloop = NuCreateLabel();
	NuPushQuitNext(exitloop, botloop);
	NuEmitLabel(irl, toploop);
        NuCompileStmtlist(irl, ast->right);
	NuEmitLabel(irl, botloop);
        NuCompileBoolBranches(irl, ast->left, toploop, NULL);
	NuEmitLabel(irl, exitloop);
	NuPopQuitNext();
	break;
    case AST_FOR:
    case AST_FORATLEASTONCE:
        NuCompileForLoop(irl, ast, ast->kind == AST_FORATLEASTONCE);
        break;
    case AST_QUITLOOP:
    case AST_ENDCASE:  /* note: in C "break" gets translated as AST_ENDCASE */
        if (!quitlabel) {
	    ERROR(ast, "loop exit statement outside of loop");
	} else {
	    NuEmitBranch(irl, NU_OP_BRA, quitlabel);
	}
        break;
    case AST_CONTINUE:
        if (!nextlabel) {
	    ERROR(ast, "loop continue statement outside of loop");
	} else {
	    NuEmitBranch(irl, NU_OP_BRA, nextlabel);
	}
	break;
    case AST_YIELD:
        /* do nothing */
        break;
    case AST_RETURN:
        if (ast->left) {
            n = NuCompileExpression(irl, ast->left);
        } else if (curfunc->numresults) {
            // RETURN without arguments, in a function with declared results,
            // gets changed to RETURN result
            n = NuCompileExpression(irl, curfunc->resultexpr);
        } else {
            // plain RETURN
            n = 0;
        }
        if (n != curfunc->numresults) {
            ERROR(ast, "number of items returned does not match function signature");
        }
        NuEmitConst(irl, NumArgLongs(curfunc));
        NuEmitConst(irl, n);
        NuEmitOp(irl, NU_OP_RET);
        break;
    case AST_FUNCCALL:
    case AST_OPERATOR:
    case AST_CAST:
    case AST_ARRAYREF:
    case AST_SEQUENCE:
    case AST_COGINIT:
    case AST_SETJMP:
    case AST_CATCH:
    case AST_MEMREF:
        n = NuCompileExpression(irl, ast);
        NuCompileDrop(irl, n);
        break;
    case AST_INTEGER:
    case AST_IDENTIFIER:
    case AST_LOCAL_IDENTIFIER:
    case AST_RESULT:
        /* do nothing */
        break;
    case AST_INLINEASM:
        NuCompileInlineAsm(irl, ast);
        break;
    case AST_THROW: {
        AST *retval = ast->left;
        int pushed;
        AST *buffer = ast->right ? ast->right : nu_abortchain_ptr;
        int flag = ast->d.ival;
        if (!retval) {
            retval = curfunc->resultexpr;
        }
        if (!retval) {
            retval = AstInteger(0);
        }
        // want to compile to __longjmp(buffer, retval, flag)
        NuCompileExpression(irl, buffer);
        pushed = NuCompileExpression(irl, retval);
        if (pushed > 1) {
            // cannot abort back multiple values!
            NuCompileDrop(irl, pushed-1);
        }
        NuEmitConst(irl, flag);
        NuEmitCommentedOp(irl, NU_OP_LONGJMP, "throw");
    } break;
    case AST_TRYENV: {
        NuCompileLhsAddress(irl, nu_abortchain_ptr);
        NuEmitCommentedOp(irl, NU_OP_LDREG, "save old abortchain ptr");
        NuCompileAlloca(irl, AstInteger(NU_JMP_BUF_SIZ));
        NuCompileLhsAddress(irl, nu_abortchain_ptr);
        NuEmitCommentedOp(irl, NU_OP_STREG, "set new abortchain ptr");
        if (ast->left && ast->left->kind != AST_STMTLIST) {
            ERROR(ast, "Nucode internal error: cannot handle expressions yet");
        } else {
            NuCompileStmtlist(irl, ast->left);
        }
        NuCompileLhsAddress(irl, nu_abortchain_ptr);
        NuEmitCommentedOp(irl, NU_OP_LDREG, "load abortchain ptr");
        NuCompileLhsAddress(irl, nu_stack_ptr);
        NuEmitCommentedOp(irl, NU_OP_STREG, "restore stack");
        NuCompileLhsAddress(irl, nu_abortchain_ptr);
        NuEmitCommentedOp(irl, NU_OP_STREG, "pop abortchain ptr");
    } break;
    case AST_BRKDEBUG: {
        int brkCode = AsmDebug_CodeGen(ast, NuDebugEval, (void *)irl);
        if (brkCode >= 0) {
            NuEmitConst(irl, brkCode);
            NuEmitOp(irl, NU_OP_BREAK);
        }
    } break;
#if 0        
    case AST_VA_START: {
        AST *va_ident = AstIdentifier("__varargs");
        NuIrOpcode op;
        int n;
        n = NuCompileExpression(irl, va_ident);
        if (n != 1) {
            ERROR(ast, "Internal error, bad varargs");
        }
        op = NuCompileLhsAddress(irl, ast->left);
        NuEmitOp(irl, op);
    } break;
#endif        
    default:
        ERROR(ast, "Unhandled node type %d in NuCompileStatement", ast->kind);
        break;
    }
}

static void
NuCompileFunction(Function *F) {
    NuIrList *irl;
    Function *saveFunc = curfunc;
    
    curfunc = F;

    NuPrepareFunctionBedata(F);
    NormalizeVarOffsets(F);
    
    // I think there's other body types so let's leave this instead of using ASSERT_AST_KIND
    if (!F->body) {
        DEBUG(NULL,"compiling function %s with no body...",F->name);
    } else if (F->body->kind == AST_STRING) {
        //WARNING(NULL,"compiling function %s which is just a reference",F->name);
        return;
    } else if (F->body->kind == AST_BYTECODE) {
        // do nothing
        curfunc = saveFunc;
        return;
    } else if (F->body->kind != AST_STMTLIST) {
        ERROR(F->body,"Internal Error: Expected AST_STMTLIST, got id %d",F->body->kind);
        curfunc = saveFunc;
        return;
    } else {
        if (F->closure) {
            // actually need variables to be in the heap
            // so insert some code to copy them
            AST *allocmem;
            AST *copymem;
            AST *args;
            AST *newbody;
            int framesize = FuncLocalSize(F) + 16;
            AST *tempvar = AstIdentifier("__interp_temp1");
            AST *supervar = AstIdentifier("__interp_super");
            // tempvar = _gc_alloc_managed(framesize)
            args = NewAST(AST_EXPRLIST, AstInteger(framesize), NULL);
            allocmem = NewAST(AST_FUNCCALL, AstIdentifier("_gc_alloc_managed"), args);
            allocmem = AstAssign(tempvar, allocmem);
            allocmem = NewAST(AST_STMTLIST, allocmem, NULL);
            // longcopy(tempvar, dbase, framesize)
            args = NewAST(AST_EXPRLIST, AstInteger(framesize), NULL);
            args = NewAST(AST_EXPRLIST, AstIdentifier("__interp_dbase"), args);
            args = NewAST(AST_EXPRLIST, tempvar, args);
            copymem = NewAST(AST_FUNCCALL, AstIdentifier("bytemove"), args);
            copymem = NewAST(AST_STMTLIST, copymem, NULL);

	    // super := vbase
            supervar = AstAssign(supervar, AstIdentifier("__interp_vbase"));
            supervar = NewAST(AST_STMTLIST, supervar, NULL);
            
            // vbase := result
            args = AstAssign(AstIdentifier("__interp_vbase"), tempvar);
            args = NewAST(AST_STMTLIST, args, F->body);
            newbody = AddToList(allocmem, copymem);
            newbody = AddToList(newbody, supervar);
            newbody = AddToList(newbody, args);
            F->body = newbody;
        }
        irl = &FunData(F)->irl;

        // emit function prologue
        NuEmitLabel(irl, FunData(F)->entryLabel);
        // ENTER needs on stack:
        //  number of return values
        //  number of arguments
        //  number of locals
        NuEmitConst(irl, NumRetLongs(F));
        NuEmitConst(irl, NumArgLongs(F));
        NuEmitConst(irl, NumLocalLongs(F));
        NuEmitOp(irl, NU_OP_ENTER);
        
        NuCompileStmtlist(irl, F->body);
        // emit function epilogue
        //NuEmitLabel(irl, FunData(F)->exitLabel);

        // verify that we ended with a RET
        if (irl->tail && irl->tail->op != NU_OP_RET) {
            if (NumRetLongs(F) == 0) {
                // just insert RET
                NuEmitConst(irl, NumArgLongs(curfunc));
                NuEmitConst(irl, 0);
                NuEmitOp(irl, NU_OP_RET);
            } else {
                // FIXME: maybe we don't actually need to do this?
                // it's possible there are already returns before this
                int n = NuCompileExpression(irl, F->resultexpr);
                NuEmitConst(irl, NumArgLongs(curfunc));
                NuEmitConst(irl, n);
                NuEmitOp(irl, NU_OP_RET);
            }
        }
    }
    curfunc = saveFunc;
}

static int NuConvertFunctions(void *vptr, Module *P) {
    Function *pf;

    NuPrepareModuleBedata(P);

    // Compile functions
    for (pf = P->functions; pf; pf = pf->next) {
        NuCompileFunction(pf);
    }
    return 1;
}

static int NuOptimizeFunction(Function *pf, NuIrList *irl) {
    // for now, do nothing
    int change;
    int all_changes = 0;
    unsigned flags = pf->optimize_flags;
    
    do {
        change = 0;
        if (flags & OPT_PEEPHOLE) {
            change += NuOptimizePeephole(irl);
            change += NuRemoveDupDrop(irl);
        }
        if (flags & OPT_DEADCODE) {
            change += NuRemoveDeadCode(irl);
        }
        all_changes += change;
    } while (change > 0);
    return all_changes;
}

static int NuRevisitFunctions(void *vptr, Module *P) {
    Module *save = current;
    NuIrList *irl;
    Function *pf;
    NuIrList **global_ptr = (NuIrList **)vptr;
    NuIrList *globalList = *global_ptr;
    current = P;

    for (pf = P->functions; pf; pf = pf->next) {
        // optimize function
        irl = &FunData(pf)->irl;
        NuOptimizeFunction(pf, irl);
        // thread into global list
        irl->nextList = globalList;
        globalList = irl;
    }

    current = save;
    *global_ptr = globalList;
    return 1;
}

static int NuCompileObject(void *vptr, Module *P) {
    struct flexbuf *fb = (struct flexbuf *)vptr;
    Function *pf;

    NuPrepareModuleBedata(P);
    if (ModData(P)->isCompiled) return 0; // already done
    
    flexbuf_printf(fb, "'--- Object: %s\n", P->classname);

        /* compile DAT block */
    if (P->datblock) {
        // Got DAT block
        // TODO pass through listing data
        Flexbuf datBuf = {0};
        Flexbuf datRelocs = {0};

        flexbuf_init(&datBuf,2048);
        flexbuf_init(&datRelocs,1024);
        PrintDataBlock(&datBuf,P->datblock,NULL,&datRelocs);
        OutputAlignLong(&datBuf);
        //NuOutputLabelNL(fb, ModData(P)->datLabel); // actually done by OutputDataBlob
        OutputDataBlob(fb, &datBuf, &datRelocs, NuLabelName(ModData(P)->datLabel));
        
        flexbuf_delete(&datRelocs);
        flexbuf_delete(&datBuf);
    }
    for (pf = P->functions; pf; pf = pf->next) {
        flexbuf_printf(fb, "'--- Function: %s\n", pf->name);
        NuOutputIrList(fb, &FunData(pf)->irl);
        if (FunData(pf)->dataLabel) {
            NuOutputLabelNL(fb, FunData(pf)->dataLabel);
            OutputDataBlob(fb, &FunData(pf)->dataBuf, NULL, NULL);
        }
    }
    return 1;
}

#ifdef NEVER
static void NuAddHeap(ByteOutputBuffer *bob, Module*P) {
    Symbol *sym = LookupSymbolInTable(&systemModule->objsyms, "__heap_base");
    Symbol *objsym = LookupSymbolInTable(&P->objsyms, "_system_");
    Label *L;

    if (! (gl_features_used & FEATURE_NEED_HEAP)) return;
    if (!objsym) return;
    if (!sym || sym->kind != SYM_LABEL) return;
    
    L = (Label *)sym->val;
    int off = L->hubval;
    //off += BCgetDAToffset(systemModule,true,NULL,false);

    sym = LookupSymbolInTable(&systemModule->objsyms, "__real_heapsize__");
    if (!sym || sym->kind != SYM_CONSTANT) return;

    uint32_t heapstart = bob->total_size + P->varsize;
    uint32_t heapsize = EvalPasmExpr((AST *)sym->val) * LONG_SIZE;

    heapsize += 4*LONG_SIZE; // reserve a slot at the end
    heapsize = (heapsize+3)&~3; // long align
    P->varsize += heapsize;

    //printf("label offset is: 0x%x heap size is %d\n", off, heapsize);

    BOB_FixupWord(bob, off, heapstart);
}
#endif

void OutputNuCode(const char *asmFileName, Module *P)
{
    FILE *asm_file;
    Module *saveCurrent = current;
    Function *saveFunc = curfunc;
    struct flexbuf asmFb;
    NuContext nuContext;
    NuIrList *globalList = 0;

    nu_stack_ptr = AstIdentifier("__interp_sp");
    nu_abortchain_ptr = AstIdentifier("__interp_abortchain");
    nu_abortresult_ptr = AstIdentifier("__interp_abortresult");
    NuIrInit(&nuContext);
    
    if (!P->functions) {
        ERROR(NULL, "Top level module has no functions");
        return;
    }
    // convert functions to IR
    VisitRecursive(NULL, P, NuConvertFunctions, VISITFLAG_COMPILEFUNCS);
    VisitRecursive(NULL, systemModule, NuConvertFunctions, VISITFLAG_COMPILEFUNCS);

    // optimize and prepare for bytecode assignment
    VisitRecursive(&globalList, P, NuRevisitFunctions, VISITFLAG_BC_OPTIMIZE);
    VisitRecursive(&globalList, systemModule, NuRevisitFunctions, VISITFLAG_BC_OPTIMIZE);
    
    // create bytecodes
    NuCreateBytecodes(globalList);

    flexbuf_init(&asmFb, 512);
    
    // find main entry point
    Function *mainFunc = GetMainFunction(P);
    if (!mainFunc) {
        ERROR(NULL,"No main function found!");
        return;
    } else if (!mainFunc->bedata) {
        ERROR(NULL,"Main function uninitialized!");
        return;
    } else if (FunData(mainFunc)->entryLabel == NULL) {
        ERROR(NULL,"Main function uncompiled!");
        return;
    }
    uint32_t clkfreq, clkmode;
    
    // set entry point and other significant variables
    if (!GetClkFreq(P, &clkfreq, &clkmode)) {
        clkfreq = 160000000;
        clkmode = 0x010007fb;
    }
    nuContext.clockFreq = clkfreq;
    nuContext.clockMode = clkmode;
    nuContext.entryPt = FunData(mainFunc)->entryLabel;
    nuContext.initSp = NuCreateLabel();
    nuContext.initObj = NuCreateLabel();
    nuContext.varSize = (P->varsize + 3) & ~3;
    
    // create & prepend interpreter
    NuOutputInterpreter(&asmFb, &nuContext);
    
    // compile objects to PASM
    VisitRecursive(&asmFb, P, NuCompileObject, VISITFLAG_EMITDAT);
    VisitRecursive(&asmFb, systemModule, NuCompileObject, VISITFLAG_EMITDAT);
    
    // finish -- heap could go here too
    NuOutputFinish(&asmFb, &nuContext);

    // output the data
    asm_file = fopen(asmFileName, "w");
    if (!asm_file) {
        ERROR(NULL, "Unable to open output file %s", asmFileName);
        return;
    }
    // emit PASM code
    flexbuf_addchar(&asmFb, 0);
    char *asmcode = flexbuf_get(&asmFb);
    fwrite(asmcode, 1, strlen(asmcode), asm_file);
    fclose(asm_file);

    current = saveCurrent;
    curfunc = saveFunc;
}
