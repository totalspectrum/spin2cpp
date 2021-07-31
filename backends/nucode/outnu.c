//
// Bytecode (nucode) compiler for spin2cpp
//
// Copyright 2021 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include "outnu.h"
#include "becommon.h"
#include <stdlib.h>

#define NumRetLongs(F) FuncLongResults(F->overalltype)
#define NumArgLongs(F) FuncLongParams(F->overalltype)
#define NumLocalLongs(F) ((F)->numlocals)

static void NuCompileStatement(NuIrList *irl, AST *ast); // forward declaration
static void NuCompileStmtlist(NuIrList *irl, AST *ast); // forward declaration
static int NuCompileExpression(NuIrList *irl, AST *ast); // returns number of items left on stack
static int NuCompileExprList(NuIrList *irl, AST *ast); // returns number of items left on stack

static void
NuPrepareObject(Module *P) {
    // Init bedata
    if (P->bedata) return;

    DEBUG(NULL,"Preparing object %s",P->fullname);

    P->bedata = calloc(sizeof(NuModData), 1);
    ModData(P)->datAddress = -1;
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

static int
NuCompileFunCall(NuIrList *irl, AST *node) {
    int pushed;
    Function *func = NULL;
    AST *functype = NULL;
    AST *objref = NULL;
    Symbol *sym;
    AST *args = node->right;
    
    pushed = NuCompileExprList(irl, args);
    sym = FindFuncSymbol(node, &objref, 1);
    if (sym && sym->kind == SYM_FUNCTION) {
        func = (Function *)sym->val;
        pushed = func->numresults;
        if (func->body && func->body->kind == AST_BYTECODE) {
            NuEmitNamedOpcode(irl, func->body->d.string);
            if (!func->result_declared) {
                pushed = 0;
            }
        } else {
            // output a CALL
            if (func->is_static || func->module == current) {
                // plain CALL is OK
                NuPrepareFunctionBedata(func);
                NuEmitAddress(irl, FunData(func)->entryLabel);
                NuEmitOp(irl, NU_OP_CALL);
            } else {
                ERROR(node, "Unable to compile method calls");
            }
        }
    } else {
        functype = ExprType(node->left);
        (void)functype;
        ERROR(node, "Unable to compile indirect function calls");
        return 0;
    }
    return pushed;
}

// find how to load a particular type, or return NU_OP_ILLEGAL if we don't know how
static NuIrOpcode LoadStoreOp(AST *typ, int isLoad)
{
    int siz = TypeSize(typ);
    NuIrOpcode op = NU_OP_ILLEGAL;
    
    switch (siz) {
    case 0:
        ERROR(NULL, "Attempt to use void value");
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
    int offset;
    
    if (!sym) {
        ERROR(node, "identifier %s not found", GetUserIdentifierName(node));
        return loadOp;
    }
    switch (sym->kind) {
    case SYM_LOCALVAR:
    case SYM_TEMPVAR:
    case SYM_PARAMETER:
    case SYM_RESULT:
        offset = sym->offset;
        loadOp = LoadStoreOp(sym->val, isLoad);
        break;
    default:
        ERROR(node, "Unhandled symbol type for %s", GetUserIdentifierName(node));
        return loadOp;
    }
    NuEmitConst(irl, offset);
    NuEmitOp(irl, offsetOp);
    return loadOp;
}

/* compile assignment */
static int
NuCompileAssign(NuIrList *irl, AST *lhs, AST *rhs, int inExpression)
{
    int n;
    NuIrOpcode op;
    
    n = NuCompileExpression(irl, rhs);
    if (n != 1) {
        ERROR(rhs, "multiple assignment not handled yet");
    }
    if (inExpression) {
        ERROR(lhs, "assignment in expression not handled yet");
        return 0;
    }
    switch (lhs->kind) {
    case AST_IDENTIFIER:
    case AST_LOCAL_IDENTIFIER:
        op = NuCompileIdentifierAddress(irl, lhs, 0);
        break;
    default:
        ERROR(lhs, "Assignment type not handled yet");
        return 0;
    }
    if (op == NU_OP_ILLEGAL) {
        ERROR(lhs, "Do not know how to store into this yet");
    } else {
        NuEmitOp(irl, op);
    }
    return 0;
}

/* returns number of longs pushed on stack */
static int
NuCompileOperator(NuIrList *irl, AST *node) {
    AST *lhs, *rhs;
    int optoken;
    int pushed = 0;
    
    lhs = node->left;
    rhs = node->right;
    optoken = node->d.ival;

    if (0) {
        // handle some special case here
    } else {
        if (lhs) {
            pushed += NuCompileExpression(irl, lhs);
        }
        if (rhs) {
            pushed += NuCompileExpression(irl, rhs);
        }
        switch (optoken) {
        case '+': NuEmitOp(irl, NU_OP_ADD); pushed = 1; break;
        case '-': NuEmitOp(irl, NU_OP_SUB); pushed = 1; break;
        default:
            ERROR(node, "Unable to handle operator");
            break;
        }
    }
    return pushed;
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
        int32_t val = EvalConstExpr(node);
        NuEmitConst(irl, val);
        return 1;
    }
    switch(node->kind) {
    case AST_IDENTIFIER:
    case AST_LOCAL_IDENTIFIER:
    {
        AST *typ;
        int siz;
        NuIrOpcode op;
        typ = ExprType(node);
        if (!typ) {
            typ = ast_type_long;
        }
        siz = TypeSize(typ);
        op = NuCompileIdentifierAddress(irl, node, 1);
        pushed = (siz+3)/4;
        if (op == NU_OP_ILLEGAL) {
            ERROR(node, "Unable to evaluate expression");
        } else {
            NuEmitOp(irl, op);
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
    default:
        ERROR(node, "Unknown expression node %d\n", node->kind);
        return 0;
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
    for (AST *ast=list;ast&&ast->kind==AST_STMTLIST;ast=ast->right) {
        AST *node = ast->left;
        if (!node) {
            //ERROR(ast,"empty node?!?"); ignore, this can happen
            continue;
        }
        NuCompileStatement(irl,node);
    }
}

static void
NuCompileStatement(NuIrList *irl, AST *ast) {
    int n;
    NuIrLabel *toploop;
    NuIrLabel *botloop;
    
    while (ast && ast->kind == AST_COMMENTEDNODE) {
        ast = ast->left;
    }
    if (!ast) return;
    switch(ast->kind) {
    case AST_COMMENT:
        // for now, do nothing
        break;

    case AST_ASSIGN:
        NuCompileAssign(irl, ast->left, ast->right, 0);
        break;
    case AST_WHILE:
        toploop = NuCreateLabel();
        botloop = NuCreateLabel();
        NuEmitLabel(irl, toploop);
        if (!IsConstExpr(ast->left)) {
            ERROR(ast, "Unable to handle conditional while");
        }
        //NuCompileBoolBranches(irl, ast->left, NULL, botloop);
        NuCompileStmtlist(irl, ast->right);
        NuEmitBranch(irl, NU_OP_BRA, toploop);
        NuEmitLabel(irl, botloop);
        break;
        
    case AST_FUNCCALL:
        n = NuCompileExpression(irl, ast);
        while (n > 0) {
            NuEmitOp(irl, NU_OP_DROP);
            --n;
        }
        break;
    case AST_RETURN:
        if (ast->left) {
            n = NuCompileExpression(irl, ast->left);
        } else {
            n = 0;
        }
        if (n != curfunc->numresults) {
            ERROR(ast, "number of items returned does not match function signature");
        }
        NuEmitConst(irl, n);
        NuEmitOp(irl, NU_OP_RET);
        break;
        
    default:
        ERROR(ast, "Unhandled node type %d in NuCompileStatement", ast->kind);
        break;
    }
}

static void
NuCompileFunction(Function *F) {
    NuIrList *irl;

    curfunc = F;

    NuPrepareFunctionBedata(F);
    NormalizeVarOffsets(F);
    
    // I think there's other body types so let's leave this instead of using ASSERT_AST_KIND
    if (!F->body) {
        DEBUG(NULL,"compiling function %s with no body...",F->name);
    } else if (F->body->kind == AST_STRING) {
        WARNING(NULL,"compiling function %s which is just a reference",F->name);
    } else if (F->body->kind == AST_BYTECODE) {
        // do nothing
        return;
    } else if (F->body->kind != AST_STMTLIST) {
        ERROR(F->body,"Internal Error: Expected AST_STMTLIST, got id %d",F->body->kind);
        return;
    } else {
        if (F->closure) {
            // actually need variables to be in the heap
            // so insert some code to copy them
            AST *allocmem;
            AST *copymem;
            AST *args;
            int framesize = FuncLocalSize(F);
            // result = _gc_alloc_managed(framesize)
            args = NewAST(AST_EXPRLIST, AstInteger(framesize), NULL);
            allocmem = NewAST(AST_FUNCCALL, AstIdentifier("_gc_alloc_managed"), args);
            allocmem = AstAssign(AstIdentifier("result"), allocmem);
            allocmem = NewAST(AST_STMTLIST, allocmem, NULL);
            // longcopy(result, dbase, framesize)
            args = NewAST(AST_EXPRLIST, AstInteger(framesize), NULL);
            args = NewAST(AST_EXPRLIST, AstIdentifier("__interp_dbase"), args);
            args = NewAST(AST_EXPRLIST, AstIdentifier("result"), args);
            copymem = NewAST(AST_FUNCCALL, AstIdentifier("bytemove"), args);
            copymem = NewAST(AST_STMTLIST, copymem, NULL);
            // dbase := result
            args = AstAssign(AstIdentifier("__interp_vbase"), AstIdentifier("result"));
            args = NewAST(AST_STMTLIST, args, F->body);
            allocmem = AddToList(allocmem, copymem);
            allocmem = AddToList(allocmem, args);
            F->body = allocmem;
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
                NuEmitConst(irl, 0);
                NuEmitOp(irl, NU_OP_RET);
            } else {
                ERROR(F->body, "Internal error, function does not end with RET");
            }
        }
    }
}

static void NuConvertFunctions(Module *P) {
    Module *save = current;
    Function *pf;
    current = P;

    NuPrepareObject(P);
    // Compile functions
    for (pf = P->functions; pf; pf = pf->next) {
        NuCompileFunction(pf);
    }

    current = save;
}

static void NuCompileObject(struct flexbuf *fb, Module *P) {
    Module *save = current;
    Function *pf;
    current = P;

    if (ModData(P)->datAddress != -1) {
        ERROR(NULL, "Already compiled object %s", P->classname);
        return;
    }
    flexbuf_printf(fb, "'--- Object: %s\n", P->classname);
    
    /* compile DAT block */
    if (P->datblock) {
        // Got DAT block
        // TODO pass through listing data
        Flexbuf datBuf;
        Flexbuf datRelocs;

        flexbuf_init(&datBuf,2048);
        flexbuf_init(&datRelocs,1024);
        const char *startLabel = NewTemporaryVariable("__Label", NULL);
        PrintDataBlock(&datBuf,P,NULL,&datRelocs);
        OutputDataBlob(fb, &datBuf, &datRelocs, startLabel);
        
        flexbuf_delete(&datRelocs);
        flexbuf_delete(&datBuf);
    }
    for (pf = P->functions; pf; pf = pf->next) {
        flexbuf_printf(fb, "'--- Function: %s\n", pf->name);
        NuOutputIrList(fb, &FunData(pf)->irl);
    }
    current = save;
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
    Module *Q;
    Module *saveCurrent = current;
    Function *saveFunc = curfunc;
    struct flexbuf asmFb;
    NuContext nuContext;
    
    NuIrInit(&nuContext);
    
    if (!P->functions) {
        ERROR(NULL, "Top level module has no functions");
        return;
    }
    // convert functions to IR
    for (Q = allparse; Q; Q = Q->next) {
        NuConvertFunctions(Q);
    }
    
    // assign opcodes to IR
    NuAssignOpcodes();

    flexbuf_init(&asmFb, 512);
    
    // find main entry point
    Function *mainFunc = GetMainFunction(P);
    if (!mainFunc) {
        ERROR(NULL,"No main function found!");
        return;
    } else if (!mainFunc->bedata) {
        ERROR(NULL,"Main function uninitialized!");
        return;
    } else if (FunData(mainFunc)->compiledAddress < 0) {
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
    
    // create & prepend interpreter
    NuOutputInterpreter(&asmFb, &nuContext);
    
    // compile objects to PASM
    for (Q = allparse; Q; Q = Q->next) {
        NuCompileObject(&asmFb, Q);
    }

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
