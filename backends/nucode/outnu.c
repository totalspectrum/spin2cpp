//
// Bytecode (nucode) compiler for spin2cpp
//
// Copyright 2021 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include "outnu.h"
#include "becommon.h"
#include <stdlib.h>

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
        if (func->body && func->body->kind == AST_BYTECODE) {
            NuEmitNamedOpcode(irl, func->body->d.string);
        } else {
            ERROR(node, "Unable to compile function call");
        }
        pushed = func->numresults;
    } else {
        functype = ExprType(node->left);
        ERROR(node, "Unable to compile indirect function calls");
        return 0;
    }
    return pushed;
}

/* returns number of longs pushed on stack */
static int
NuCompileExpression(NuIrList *irl, AST *node) {
    int pushed = 0; // number results pushed on stack
    if(!node) {
        ERROR(NULL,"Internal Error: Null expression!!");
        return pushed;
    }
    if (IsConstExpr(node)) {
        int32_t val = EvalConstExpr(node);
        NuEmitConst(irl, val);
        return 1;
    }
    switch(node->kind) {
    case AST_FUNCCALL:
    {
        pushed = NuCompileFunCall(irl, node);
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
NuCompileStatement(NuIrList *irl, AST *node) {
    int n;
    while (node && node->kind == AST_COMMENTEDNODE) {
        node = node->left;
    }
    if (!node) return;
    switch(node->kind) {
    case AST_COMMENT:
        // for now, do nothing
        break;

    case AST_FUNCCALL:
        n = NuCompileExpression(irl, node);
        while (n > 0) {
            NuEmitOp(irl, NU_OP_DROP);
            --n;
        }
        break;
    default:
        ERROR(node, "Unhandled node type %d in NuCompileStatement", node->kind);
        break;
    }
}

static void
NuCompileFunction(Function *F) {
    NuIrList *irl;

    curfunc = F;

    if (F->bedata) {
        ERROR(NULL, "function %s already has back end data?", F->name);
        return;
    }
    F->bedata = calloc(sizeof(NuFunData),1);

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

        FunData(F)->entryLabel = NuCreateLabel();
        FunData(F)->exitLabel = NuCreateLabel();
        
        // emit function prologue
        NuEmitLabel(irl, FunData(F)->entryLabel);
        NuEmitOp(irl, NU_OP_ENTER);
        NuCompileStmtlist(irl, F->body);
        // emit function epilogue
        NuEmitLabel(irl, FunData(F)->exitLabel);
        NuEmitOp(irl, NU_OP_RET);
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

void OutputNuCode(const char *fname, Module *P)
{
    FILE *asm_file;
    FILE *bin_file;
    FILE *lst_file = NULL;
    Module *Q;
    struct flexbuf asmFb;
    
    NuIrInit();
    
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
    
    // create & prepend interpreter
    NuOutputInterpreter(&asmFb);
    
    // compile objects to PASM
    for (Q = allparse; Q; Q = Q->next) {
        NuCompileObject(&asmFb, Q);
    }

    // Align and append any needed heap
    //NuAddHeap(&bob,P);

    // add main entry point
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
    // FIXME: put entry point into BOB
    const char *asmFileName = ReplaceExtension(fname, ".p2asm");
    asm_file = fopen(asmFileName, "w");
    if (!asm_file) {
        ERROR(NULL, "Unable to open output file %s", asmFileName);
        return;
    }
    if (gl_listing) {
        lst_file = fopen(ReplaceExtension(fname, ".lst"), "w");
    }


    // emit PASM code
    flexbuf_addchar(&asmFb, 0);
    char *asmcode = flexbuf_get(&asmFb);
    fwrite(asmcode, 1, strlen(asmcode), asm_file);
    fclose(asm_file);

    if (lst_file) fclose(lst_file);
}
