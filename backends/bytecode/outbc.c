

#include "outbc.h"
#include "bcbuffers.h"
#include "bcir.h"
#include <stdlib.h>


struct bcheaderspans {
    OutputSpan *pbase,*vbase,*dbase,*pcurr,*dcurr;
};

static struct bcheaderspans
OutputSpinBCHeader(ByteOutputBuffer *bob, Module *P)
{
    unsigned int clkfreq;
    unsigned int clkmodeval;

    struct bcheaderspans spans = {0};

    if (!GetClkFreq(P, &clkfreq, &clkmodeval)) {
        // use defaults
        clkfreq = 80000000;
        clkmodeval = 0x6f;
    }
    
    BOB_PushLong(bob, clkfreq, "CLKFREQ");     // offset 0
    
    BOB_PushByte(bob, clkmodeval, "CLKMODE");  // offset 4
    BOB_PushByte(bob, 0,"Placeholder for checksum");      // checksum
    spans.pbase = BOB_PushWord(bob, 0x0010,"PBASE"); // PBASE
    
    spans.vbase = BOB_PushWord(bob, 0x7fe8,"VBASE"); // VBASE offset 8
    spans.dbase = BOB_PushWord(bob, 0x7ff0,"DBASE"); // DBASE
    
    spans.pcurr = BOB_PushWord(bob, 0x0018,"PCURR"); // PCURR  offset 12
    spans.dcurr = BOB_PushWord(bob, 0x7ff8,"DCURR"); // DCURR

    return spans;
}

// TODO: Integrate this with how it's supposed to work
static int
HWReg2Index(HwReg *reg) {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM: {
        int indexBase = 0x1E0;
             if (!strcmp(reg->name,"par")) return 0x1F0 - indexBase;
        else if (!strcmp(reg->name,"cnt")) return 0x1F1 - indexBase;
        else if (!strcmp(reg->name,"ina")) return 0x1F2 - indexBase;
        else if (!strcmp(reg->name,"inb")) return 0x1F3 - indexBase;
        else if (!strcmp(reg->name,"outa")) return 0x1F4 - indexBase;
        else if (!strcmp(reg->name,"outb")) return 0x1F5 - indexBase;
        else if (!strcmp(reg->name,"dira")) return 0x1F6 - indexBase;
        else if (!strcmp(reg->name,"dirb")) return 0x1F7 - indexBase;
        else if (!strcmp(reg->name,"ctra")) return 0x1F8 - indexBase;
        else if (!strcmp(reg->name,"ctrb")) return 0x1F9 - indexBase;
        else if (!strcmp(reg->name,"frqa")) return 0x1FA - indexBase;
        else if (!strcmp(reg->name,"frqb")) return 0x1FB - indexBase;
        else if (!strcmp(reg->name,"phsa")) return 0x1FC - indexBase;
        else if (!strcmp(reg->name,"phsb")) return 0x1FD - indexBase;
        else if (!strcmp(reg->name,"vcfg")) return 0x1FE - indexBase;
        else if (!strcmp(reg->name,"vscl")) return 0x1FF - indexBase;
        else {
            ERROR(NULL,"Unknown register %s",reg->name);
            return 0;
        }
    } break;
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return 0;
    }
}

static void
printASTInfo(AST *node) {
    if (node) {
        printf("node is %d, ",node->kind);
        if (node->left) printf("left is %d, ",node->left->kind);
        else printf("left empty, ");
        if (node->right) printf("right is %d\n",node->right->kind);
        else printf("right empty\n");
    } else printf("null node");
}

#define ASSERT_AST_KIND(node,expectedkind,whatdo) {\
if (!node) { ERROR(NULL,"Internal Error: Expected " #expectedkind " got NULL\n"); whatdo ; } \
else if (node->kind != expectedkind) { ERROR(node,"Internal Error: Expected " #expectedkind " got %d\n",node->kind); whatdo ; }}

static void BCCompileExpression(BCIRBuffer *irbuf,AST *node,BCContext context); // forward decl;


static void
BCCompileMemOp(BCIRBuffer *irbuf,AST *node,BCContext context, enum ByteOpKind kind) {
    ByteOpIR memOp = {0};
    memOp.kind = kind;

    AST *type = NULL;

    printASTInfo(node);
    AST *ident = (node->kind == AST_ARRAYREF) ? node->left : node;
    if (IsIdentifier(ident)) {
        Symbol *sym = LookupAstSymbol(ident,NULL);
        if (!sym) ERROR(ident,"Can't get symbol");
        else printf("got symbol with name %s and kind %d\n",sym->our_name,sym->kind);
        type = ExprType(ident);
        switch (sym->kind) {
        case SYM_LABEL: {
            printf("Got SYM_LABEL (DAT symbol)... ");
            memOp.attr.memop.base = MEMOP_BASE_PBASE;

            Label *lab = sym->val;
            uint32_t labelval = lab->hubval;
            printf("Value inside is %d... ",labelval);
            // Add header offset
            labelval += 4*(ModData(current)->pub_cnt+ModData(current)->pri_cnt+ModData(current)->obj_cnt+1);
            printf("After header offset: %d\n",labelval);
            memOp.data.int32 = labelval;
        } break;
        case SYM_VARIABLE: {
            if (!type) type = ast_type_long; // FIXME this seems wrong, but is NULL otherwise
            printf("Got SYM_VARIABLE (VAR symbol)... ");
            memOp.attr.memop.base = MEMOP_BASE_VBASE;
            printf("sym->offset is %d\n",sym->offset);
            memOp.data.int32 = sym->offset;

        } break;
        default:
            ERROR(ident,"Unhandled Symbol type: %d",sym->kind);
            return;
        }
    } else {
        ERROR(node,"Identifier is not identifier (yeah)");
        return;
    }
    
    printf("Got type ");printASTInfo(type);
    if (type && type->kind == AST_ARRAYTYPE) type = type->left; // We don't care if this is an array, we just want the size

         if (type == ast_type_byte) memOp.attr.memop.memSize = MEMOP_SIZE_BYTE;
    else if (type == ast_type_word) memOp.attr.memop.memSize = MEMOP_SIZE_WORD; 
    else if (type == ast_type_long) memOp.attr.memop.memSize = MEMOP_SIZE_LONG;
    else ERROR(node,"Can't figure out mem op type (%s)",type?"is unhandled":"is NULL");

    BIRB_PushCopy(irbuf,&memOp);
}

static void
BCCompileAssignment(BCIRBuffer *irbuf,AST *node,BCContext context,bool asExpression) {
    AST *left = node->left, *right = node->right;
    printf("Got an assign! left kind: %d, right kind %d\n",left->kind,right->kind);

    if (asExpression) ERROR(node,"Assignment expression NYI");  

    BCCompileExpression(irbuf,right,context);

    switch(left->kind) {
    case AST_HWREG: {
        HwReg *hw = (HwReg *)left->d.ptr;
        printf("Got HWREG assign, %s = %d\n",hw->name,HWReg2Index(hw));
        ByteOpIR hwAssignOp = {0};
        hwAssignOp.kind = BOK_REG_WRITE;
        hwAssignOp.data.int32 = HWReg2Index(hw);
        BIRB_PushCopy(irbuf,&hwAssignOp);
    } break;
    case AST_ARRAYREF: {
        printf("Got ARRAYREF assign\n");
        BCCompileMemOp(irbuf,node->left,context,BOK_MEM_WRITE);
    } break;
    case AST_IDENTIFIER: {
        printf("Got IDENTIFIER assign\n");
        Symbol *sym = LookupAstSymbol(left,NULL);
        if (!sym) {
            ERROR(node,"Internal Error: no symbol");
            return;
        }
        switch (sym->kind) {
        case SYM_VARIABLE:
            BCCompileMemOp(irbuf,left,context,BOK_MEM_WRITE);
            break;
        default:
            ERROR(left,"Unhandled Identifier symbol kind %d",sym->kind);
            return;
        }
    } break;
    default:
        ERROR(left,"Unhandled assign left kind %d",left->kind);
        break;
    }

}

static ByteOpIR*
BCNewOrphanLabel() {
    ByteOpIR *lbl = calloc(sizeof(ByteOpIR),1);
    lbl->kind = BOK_LABEL;
    return lbl;
}

static ByteOpIR*
BCPushLabel(BCIRBuffer *irbuf) {
    ByteOpIR lbl = {0};
    lbl.kind = BOK_LABEL;
    return BIRB_PushCopy(irbuf,&lbl);
}

static void
BCCompileInteger(BCIRBuffer *irbuf,int32_t ival) {
    ByteOpIR opc = {0};
    opc.kind = BOK_CONSTANT;
    opc.data.int32 = ival;
    BIRB_PushCopy(irbuf,&opc);
}

// Very TODO
static void
BCCompileFunCall(BCIRBuffer *irbuf,AST *node,BCContext context, bool asExpression) {
    printf("Compiling fun call, ");
    printASTInfo(node);

    Symbol *sym = NULL;
    if (node->left && node->left->kind == AST_METHODREF) {
        ERROR(node,"Method calls NYI");
        return;
    } else {
        sym = LookupAstSymbol(node->left, NULL);
    }

    ByteOpIR callOp = {0};
    if (!sym) {
        ERROR(node,"Function call has no symbol");
        return;
    } else if (sym->kind == SYM_BUILTIN) {
        if (!strcmp(sym->our_name,"waitcnt")) {
            callOp.kind = BOK_WAIT;
            callOp.attr.wait.type = BCW_WAITCNT;
        } else {
            ERROR(node,"Unhandled builtin %s",sym->our_name);
            return;
        }
    } else {
        ERROR(node,"Non-builtin functions NYI (name is %s and sym kind is %d)",sym->our_name,sym->kind);
        return;
    }

    ASSERT_AST_KIND(node->right,AST_EXPRLIST,return;);
    for (AST *list=node->right;list;list=list->right) {
        printf("Compiling function call argument...");
        ASSERT_AST_KIND(list,AST_EXPRLIST,return;);
        BCCompileExpression(irbuf,list->left,context);
    }


    BIRB_PushCopy(irbuf,&callOp);

    // TODO pop unwanted results????

}

static void
BCCompileExpression(BCIRBuffer *irbuf,AST *node,BCContext context) {
    switch(node->kind) {
        case AST_INTEGER: {
            printf("Got integer %d\n",node->d.ival);
            BCCompileInteger(irbuf,node->d.ival);
        } break;
        case AST_CONSTANT: {
            printf("Got constant, ");
            int32_t evald = EvalConstExpr(node);
            printf("eval'd to %d\n",evald);
            BCCompileInteger(irbuf,evald);
        } break;
        case AST_ASSIGN: {
            printf("Got assignment in expression \n");
            BCCompileAssignment(irbuf,node,context,true);
        } break;
        case AST_OPERATOR: {
            printf("Got operator in expression: %d\n",node->d.ival);
            enum MathOpKind mok;
            bool unary = false;
            switch(node->d.ival) {
            case '^': mok = MOK_BITXOR; break;
            case '|': mok = MOK_BITOR; break;
            case '&': mok = MOK_BITAND; break;
            case '+': mok = MOK_ADD; break;
            case '-': mok = MOK_SUB; break;
            case K_SHR: mok = MOK_SHR; break;
            case K_SHL: mok = MOK_SHL; break;
            default:
                ERROR(node,"Unhandled operator %03X",node->d.ival);
                mok = MOK_UHHH;
                break;
            }

            printf("Operator resolved to %d = %s\n",mok,mathOpKindNames[mok]);

            if (unary) ERROR(node,"Unary operators NYI");
            BCCompileExpression(irbuf,node->left,context);
            BCCompileExpression(irbuf,node->right,context);

            ByteOpIR mathOp = {0};
            mathOp.kind = BOK_MATHOP;
            mathOp.mathKind = mok;
            BIRB_PushCopy(irbuf,&mathOp);
        } break;
        case AST_HWREG: {
            HwReg *hw = node->d.ptr;
            printf("Got hwreg in expression: %s\n",hw->name);
            ByteOpIR hwread = {0};
            hwread.kind = BOK_REG_READ;
            hwread.data.int32 = HWReg2Index(hw);
            BIRB_PushCopy(irbuf,&hwread);
        } break;
        case AST_ADDROF: {
            printf("Got addr-of! ");
            printASTInfo(node); // Right always empty, left always ARRAYREF?
            BCCompileMemOp(irbuf,node->left,context,BOK_MEM_ADDRESS);
        } break;
        case AST_IDENTIFIER: {
            printf("Got identifier! ");
            Symbol *sym = LookupAstSymbol(node,NULL);
            if (!sym) {
                ERROR(node,"Internal Error: no symbol");
                return;
            }
            switch (sym->kind) {
            case SYM_VARIABLE:
                BCCompileMemOp(irbuf,node,context,BOK_MEM_READ);
                break;
            default:
                ERROR(node,"Unhandled Identifier symbol kind %d",sym->kind);
                return;
            }

        } break;
        case AST_ARRAYREF: {
            printf("Got array read! ");
            printASTInfo(node);
            BCCompileMemOp(irbuf,node,context,BOK_MEM_READ);
        } break;
        default:
            ERROR(node,"Unhandled node kind %d in expression",node->kind);
            break;
    }
}

static void 
BCCompileStmtlist(BCIRBuffer *irbuf,AST *list, BCContext context) {
    printf("Compiling a statement list...\n");

    for (AST *ast=list;ast&&ast->kind==AST_STMTLIST;ast=ast->right) {
        AST *node = ast->left;
        if (!node) {
            ERROR(ast,"empty node?!?");
            continue;
        }
        while (node->kind == AST_COMMENTEDNODE) {
            //printf("Node is allegedly commented:\n");
            // FIXME: this doesn't actually get any comments?
            for(AST *comment = node->right;comment&&comment->kind==AST_COMMENT;comment=comment->right) {
                printf("---  %s\n",comment->d.string);
            }
            node = node->left;
        }

        switch(node->kind) {
        case AST_ASSIGN:
            BCCompileAssignment(irbuf,node,context,false);
            break;
        case AST_WHILE: {
            printf("Got While loop.. ");
            printASTInfo(node);

            ByteOpIR *toplbl = BCPushLabel(irbuf);
            ByteOpIR *bottomlbl = BCNewOrphanLabel();

            // Compile condition
            BCCompileExpression(irbuf,node->left,context);
            ByteOpIR condjmp = {0};
            condjmp.kind = BOK_JUMP_IF_Z;
            condjmp.data.jumpTo = bottomlbl;
            BIRB_PushCopy(irbuf,&condjmp);

            ASSERT_AST_KIND(node->right,AST_STMTLIST,break;)

            BCContext newcontext = context;
            BCCompileStmtlist(irbuf,node->right,newcontext);

            ByteOpIR returnjmp = {0};
            returnjmp.kind = BOK_JUMP;
            returnjmp.data.jumpTo = toplbl;
            BIRB_PushCopy(irbuf,&returnjmp);
            BIRB_Push(irbuf,bottomlbl);

            
        } break;
        case AST_FUNCCALL: {
            BCCompileFunCall(irbuf,node,context,false);
        } break;
        default:
            ERROR(node,"Unhandled node kind %d",node->kind);
            break;
        }
    }

}

static void 
BCCompileFunction(ByteOutputBuffer *bob,Function *F) {

    printf("Compiling bytecode for function %s\n",F->name);
    curfunc = F; // Set this global, I guess;

    // first up, we now know the function's address, so let's fix it up in the header
    int func_ptr = bob->total_size;
    int func_offset = 0;
    int func_localsize = F->local_var_counter*4; // SUPER DUPER FIXME
    Module *M = F->module;
    if (M->bedata == NULL || ModData(M)->compiledAddress < 0) {
        ERROR(NULL,"Compiling function on Module whose address is not yet determined???");
    } else func_offset = func_ptr - ModData(M)->compiledAddress;

    if(F->bedata == NULL || !FunData(F)->headerEntry) {
        ERROR(NULL,"Compiling function that does not have a header entry");
        return;
    }
    
    FunData(F)->compiledAddress = func_ptr;

    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        BOB_ReplaceLong(FunData(F)->headerEntry,
            (func_offset&0xFFFF) + 
            ((func_localsize)<<16),
            auto_printf(128,"Function %s @%04X (local size %d)",F->name,func_ptr,func_localsize)
        );
        break;
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return;
    }

    BOB_Comment(bob,auto_printf(128,"--- Function %s",F->name));
    // Compile function
    BCIRBuffer irbuf = {0};


    // I think there's other body types so let's leave this instead of using ASSERT_AST_KIND
    if (!F->body) {
        ERROR(F->body,"Internal Error: Function has no body (TODO: generate return)");
        return;
    } else if (F->body->kind != AST_STMTLIST) {
        ERROR(F->body,"Internal Error: Expected AST_STMTLIST, got id %d",F->body->kind);
        return;
    } else {
        BCContext context = {0};
        BCCompileStmtlist(&irbuf,F->body,context);
    }
    // Always append a return (TODO: only when neccessary)
    ByteOpIR retop = {0,.kind = BOK_RETURN_PLAIN};
    BIRB_PushCopy(&irbuf,&retop);

    BCIR_to_BOB(&irbuf,bob);
    
    curfunc = NULL;
}

static void
BCCompileObject(ByteOutputBuffer *bob, Module *P) {

    printf("Compiling bytecode for object %s\n",P->fullname);

    BOB_Align(bob,4); // Long-align

    BOB_Comment(bob,auto_printf(128,"--- Object Header for %s",P->classname));

    // Init bedata
    if (!P->bedata) {
        P->bedata = calloc(sizeof(BCModData), 1);
        ModData(P)->compiledAddress = -1;
    }
    if (ModData(P)->compiledAddress < 0) {
        ModData(P)->compiledAddress = bob->total_size;
    } else {
        ERROR(NULL,"Already compiled module %s",P->fullname);
        return;
    }

    // Count and gather private/public methods
    {
        int pub_cnt = 0, pri_cnt = 0;
        for (Function *f = P->functions; f; f = f->next) {
            if (ShouldSkipFunction(f)) continue;

            if (f->is_public) ModData(P)->pubs[pub_cnt++] = f;
            else              ModData(P)->pris[pri_cnt++] = f;

            printf("Got function %s\n",f->user_name);

            if (pub_cnt+pri_cnt >= BC_MAX_POINTERS) {
                ERROR(NULL,"Too many functions in Module %s",P->fullname);
                return;
            }
        }
        ModData(P)->pub_cnt = pub_cnt;
        ModData(P)->pri_cnt = pri_cnt;
    }
    // Count can't be modified anymore, so mirror it into const locals for convenience
    const int pub_cnt = ModData(P)->pub_cnt;
    const int pri_cnt = ModData(P)->pri_cnt;

    // Count and gather object members - (TODO does this actually work?)
    {
        int obj_cnt = 0;
        for (AST *upper = P->finalvarblock; upper; upper = upper->right) {
            if (upper->kind != AST_LISTHOLDER) {
                ERROR(upper, "internal error: expected listholder");
                return;
            }
            AST *var = upper->left;
            if (!(var->kind = AST_DECLARE_VAR && IsClassType(var->left))) continue;

            // Debug gunk
            ASSERT_AST_KIND(var->left,AST_OBJECT,);
            ASSERT_AST_KIND(var->right,AST_LISTHOLDER,);
            ASSERT_AST_KIND(var->right->left,AST_IDENTIFIER,);
            printf("Got obj of type %s named %s\n",((Module*)var->left->d.ptr)->classname,var->right->left->d.string);

            ModData(P)->objs[obj_cnt++] = var;

            if (pub_cnt+pri_cnt+obj_cnt >= BC_MAX_POINTERS) {
                ERROR(NULL,"Too many objects in Module %s",P->fullname);
                return;
            }
        }
        ModData(P)->obj_cnt = obj_cnt;
    }
    const int obj_cnt = ModData(P)->obj_cnt;

    printf("Debug: this object (%s) has %d PUBs, %d PRIs and %d OBJs\n",P->classname,pub_cnt,pri_cnt,obj_cnt);

    // Emit object header
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        BOB_PushWord(bob,0,"Object size(?)"); // (TODO)
        BOB_PushByte(bob,pri_cnt+pub_cnt+1,"Method count + 1");
        BOB_PushByte(bob,obj_cnt, "OBJ count");

        for (int i=0;i<(pri_cnt+pub_cnt);i++) {
            Function *fun = i>=pub_cnt ? ModData(P)->pris[i-pub_cnt] : ModData(P)->pubs[i];

            if (fun->bedata == NULL) {
                fun->bedata = calloc(sizeof(BCFunData),1);
            }
            if (FunData(fun)->headerEntry != NULL) ERROR(NULL,"function %s already has a header entry????",fun->name);
            OutputSpan *span = BOB_PushLong(bob,0,"!!!Uninitialized function!!!");
            FunData(fun)->headerEntry = span;
        }

        for (int i=0;i<obj_cnt;i++) {
            BOB_PushWord(bob,0,"Header offset?"); // Header Offset?
            BOB_PushWord(bob,0,"VAR offset?"); // VAR Offset (from current object's VAR base)
        }

        break;
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return;
    }

    if (P->datblock) {
        // Got DAT block
        // TODO pass through listing data
        Flexbuf datBuf;
        flexbuf_init(&datBuf,2048);
        PrintDataBlock(&datBuf,P,NULL,NULL);
        BOB_Comment(bob,"--- DAT Block");
        BOB_Push(bob,(uint8_t*)flexbuf_peek(&datBuf),flexbuf_curlen(&datBuf),NULL);
        flexbuf_delete(&datBuf);
    }

    // Compile functions
    for(int i=0;i<pub_cnt;i++) BCCompileFunction(bob,ModData(P)->pubs[i]);
    for(int i=0;i<pri_cnt;i++) BCCompileFunction(bob,ModData(P)->pris[i]);

    // emit subobjects
    for (int i=0;i<obj_cnt;i++) {
        Module *mod = ModData(P)->objs[i]->left->d.ptr;
        if (mod->bedata == NULL || ((BCModData*)mod->bedata)->compiledAddress < 0) {
            BCCompileObject(bob,mod);

        }
    }

    return; 
}

void OutputByteCode(const char *fname, Module *P) {
    FILE *bin_file = NULL,*lst_file = NULL;
    Module *save;
    ByteOutputBuffer bob = {0};

    save = current;
    current = P;

    printf("Debug: In OutputByteCode\n");
    BCIR_Init();

    if (!P->functions) {
        ERROR(NULL,"Can't compile code without functions");
    }

    // Emit header / interp ASM
    struct bcheaderspans headerspans = {0};
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        headerspans = OutputSpinBCHeader(&bob,P);
        break;
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return;
    }

    BCCompileObject(&bob,P);

    BOB_Align(&bob,4);
    const int programSize = bob.total_size;


    // Fixup header
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        // TODO fixup everything
        BOB_ReplaceWord(headerspans.pcurr,FunData(ModData(P)->pubs[0])->compiledAddress,NULL);
        BOB_ReplaceWord(headerspans.vbase,programSize,NULL);
        break;
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return;
    }

    bin_file = fopen(fname,"wb");
    if (gl_listing) lst_file = fopen(ReplaceExtension(fname,".lst"),"w"); // FIXME: Get list file name from cmdline

    // Walk through buffer and emit the stuff
    int outPosition = 0;
    const int maxBytesPerLine = 8;
    const int listPadColumn = (gl_p2?5:4)+maxBytesPerLine*3+2;
    for(OutputSpan *span=bob.head;span;span = span->next) {
        if (span->size > 0) fwrite(span->data,span->size,1,bin_file);

        // Handle listing output
        if (gl_listing) {
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

    current = save;
}
