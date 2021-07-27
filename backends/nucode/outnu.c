//
// Bytecode (nucode) compiler for spin2cpp
//
// Copyright 2021 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include "outnu.h"
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

static void
NuDatPutLong(uint8_t *ptr, uint32_t data) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    data = __builtin_bswap32(data);
#endif
    memcpy(ptr, &data, 4);
}

static void
NuProcessDatRelocs(Module *P, OutputSpan *span, Reloc *reloc, size_t numRelocs)
{
    size_t i;
    int datbase = ModData(P)->datAddress;
    uint8_t *spdata = span->data;
    size_t maxsize = span->size;

    if (datbase < 0) {
        ERROR(NULL, "Unable to get DAT base for %s", P->classname);
        return;
    }
    for (i = 0; i < numRelocs; i++, reloc++) {
        switch (reloc->kind) {
        case RELOC_KIND_I32: {
            Symbol *sym = reloc->sym;
            int off = reloc->addr;
            uint32_t data;

            if (off + 4 > maxsize) {
                ERROR(NULL, "Relocation goes past end of span");
                return;
            }
            if (!sym) {
                // relocation relative to start of dat
                data = reloc->symoff + datbase;
                NuDatPutLong(&spdata[off], data);
            } else {
                ERROR(NULL, "Unable to handle relocation for %s", sym->user_name);
            }
        } break;
        case RELOC_KIND_NONE:
        case RELOC_KIND_DEBUG:
            break; /* nothing to do */
        case RELOC_KIND_AUGS:
        case RELOC_KIND_AUGD:
        default:
            ERROR(NULL, "Unable to handle reloc %d in bytecode", reloc->kind);
            break;
        }
    }
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

static void NuCompileObject(ByteOutputBuffer *bob, Module *P) {
    Module *save = current;
    Function *pf;
    current = P;

    if (ModData(P)->datAddress != -1) {
        ERROR(NULL, "Already compiled object %s", P->classname);
        return;
    }
    BOB_Align(bob, LONG_SIZE);
    BOB_Comment(bob, auto_printf(128, "--- Object: %s", P->classname));

    ModData(P)->datAddress = bob->total_size;
    
    /* compile DAT block */
    if (P->datblock) {
        // Got DAT block
        // TODO pass through listing data
        Flexbuf datBuf;
        Flexbuf datRelocs;
        OutputSpan *datSpan;
        flexbuf_init(&datBuf,2048);
        flexbuf_init(&datRelocs,1024);

        PrintDataBlock(&datBuf,P,NULL,&datRelocs);
        BOB_Comment(bob,"--- DAT Block");
        datSpan = BOB_Push(bob,(uint8_t*)flexbuf_peek(&datBuf),flexbuf_curlen(&datBuf),NULL);
        NuProcessDatRelocs(P, datSpan, (Reloc*)flexbuf_peek(&datRelocs), flexbuf_curlen(&datRelocs)/sizeof(Reloc));
        flexbuf_delete(&datRelocs);
        flexbuf_delete(&datBuf);
    }
    
    current = save;
}

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

void OutputNuCode(const char *fname, Module *P)
{
    FILE *bin_file;
    FILE *lst_file = NULL;
    Module *Q;
    ByteOutputBuffer bob = {0};

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
    
    // create & prepend interpreter

    // compile objects to binary
    for (Q = allparse; Q; Q = Q->next) {
        NuCompileObject(&bob, Q);
    }

    // Align and append any needed heap
    BOB_Align(&bob,4);
    NuAddHeap(&bob,P);

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

    bin_file = fopen(fname, "wb");
    if (!bin_file) {
        ERROR(NULL, "Unable to open output file %s", fname);
        return;
    }
    if (gl_listing) {
        lst_file = fopen(ReplaceExtension(fname, ".lst"), "w");
    }

    // Walk through buffer and emit the stuff
    int outPosition = 0;
    const int maxBytesPerLine = 8;
    const int listPadColumn = (gl_p2?5:4)+maxBytesPerLine*3+2;
    for(OutputSpan *span=bob.head;span;span = span->next) {
        if (span->size > 0) fwrite(span->data,span->size,1,bin_file);

        // Handle listing output
        if (lst_file) {
            if (span->size == 0) {
                if (span->comment) fprintf(lst_file,"'%s\n",span->comment);
            } else {
                int listPosition = 0;
                while (listPosition < span->size) {
                    int listPosition_curline = listPosition; // Position at beginning of line
                    int list_linelen = fprintf(lst_file,gl_p2 ? "%05X: " : "%04X: ",outPosition+listPosition_curline);
                    // Print up to 8 bytes per line
                    for (;(listPosition-listPosition_curline) < maxBytesPerLine && listPosition<span->size;listPosition++) {
                        list_linelen += fprintf(lst_file,"%02X ",span->data[listPosition]);
                    }
                    // Print comment, indented
                    if (span->comment) {
                        for(;list_linelen<listPadColumn;list_linelen++) fputc(' ',lst_file);
                        if (listPosition_curline == 0) { // First line? (of potential multi-line span)
                            fprintf(lst_file,"' %s",span->comment);
                        } else {
                            fprintf(lst_file,"' ...");
                        }
                    }
                    fputc('\n',lst_file);
                }
                
            }
        }

        outPosition += span->size;
    }

    fclose(bin_file);
    if (lst_file) fclose(lst_file);
}
