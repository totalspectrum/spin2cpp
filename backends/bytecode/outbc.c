

#include "outbc.h"
#include "bcbuffers.h"
#include "bcir.h"
#include <stdlib.h>


static bool interp_can_multireturn() {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM: return false;
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return false;
    }
}

static int BCResultsBase() {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM: return 0;
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return 0;
    }
}

static int BCParameterBase() {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM: return 4; // RESULT is always there
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return 0;
    }
}

static int BCLocalBase() {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM: return 4+curfunc->numparams*4; // FIXME small variables
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return 0;
    }
}

static int BCGetOBJSize(Module *P,AST *ast) {
    if (ast->kind == AST_LISTHOLDER) ast = ast->right;
    if (ast->kind == AST_DECLARE_VAR) {
        Module *mod = GetClassPtr(ast->left);
        if (!mod) {
            ERROR(ast,"Can't get Module in BCGetOBJSize");
            return 0;
        }
        return mod->varsize;
    } else printf("Unhandled AST Kind %d in BCGetOBJSize\n",ast->kind);
    return 0;
}

static int BCGetOBJOffset(Module *P,AST *ast) {
    Symbol *sym = NULL;
    if (ast->kind == AST_LISTHOLDER) ast = ast->right;
    if (ast->kind == AST_DECLARE_VAR) {
        // FIXME this seems kindof wrong?
        printf("AST_DECLARE_VAR\n");
        AST *ident = ast->right->left;
        if (ident->kind == AST_ARRAYDECL) ident = ident->left;
        if (ident->kind != AST_IDENTIFIER) {
            ERROR(ident,"Not an identifier");
            return 0;
        }
        const char *name = ident->d.string;
        printf("Looking up %s in a module\n",name);
        if(name) sym = LookupSymbolInTable(&P->objsyms,name);
    } else if (ast->kind == AST_IDENTIFIER) {
        printf("AST_IDENTIFIER\n");
        sym = LookupAstSymbol(ast,NULL);
    } else printf("Unhandled AST Kind %d in BCGetOBJOffset\n",ast->kind);
    if (!sym) {
        ERROR(ast,"Can't find symbol for an OBJ");
        return -1;
    }
    printf("In BCGetOBJOffset: ");
    printf("got symbol with name %s, kind %d, offset %d\n",sym->our_name,sym->kind,sym->offset);
    return sym->offset;
}

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

static const void StringAppend(Flexbuf *fb,AST *expr) {
    if(!expr) return;
    switch (expr->kind) {
    case AST_INTEGER: {
        int i = expr->d.ival;
        if (i < 0 || i>255) ERROR(expr,"Character out of range!");
        flexbuf_putc(i,fb);
    } break;
    case AST_STRING: {
        flexbuf_addstr(fb,expr->d.string);
    } break;
    case AST_EXPRLIST: {
        if (expr->left) StringAppend(fb,expr->left);
        if (expr->right) StringAppend(fb,expr->right);
    } break;
    default: {
        ERROR(expr,"Unhandled AST kind %d in string expression",expr->kind);
    } break;
    }
}

ByteOpIR BCBuildString(AST *expr) {
    ByteOpIR ir = {0};
    ir.kind = BOK_FUNDATA_STRING;
    Flexbuf fb;
    flexbuf_init(&fb,20);
    StringAppend(&fb,expr);
    flexbuf_putc(0,&fb);
    ir.data.stringPtr = flexbuf_peek(&fb);
    ir.attr.stringLength = flexbuf_curlen(&fb);
    return ir;
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


#define ASSERT_AST_KIND_LOC2(f,l) f ":" #l
#define ASSERT_AST_KIND_LOC(f,l) ASSERT_AST_KIND_LOC2(f,l)
#define ASSERT_AST_KIND(node,expectedkind,whatdo) {\
if (!node) { ERROR(NULL,"Internal Error: Expected " #expectedkind " got NULL @" ASSERT_AST_KIND_LOC(__FILE__,__LINE__) "\n"); whatdo ; } \
else if (node->kind != expectedkind) { ERROR(node,"Internal Error: Expected " #expectedkind " got %d @" ASSERT_AST_KIND_LOC(__FILE__,__LINE__) "\n",node->kind); whatdo ; }}
 
static void BCCompileExpression(BCIRBuffer *irbuf,AST *node,BCContext context,bool asStatement); // forward decl;
static void BCCompileStatement(BCIRBuffer *irbuf,AST *node, BCContext context); // forward decl;

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

static void
BCCompileJumpEx(BCIRBuffer *irbuf, ByteOpIR *label, enum ByteOpKind kind) {
    ByteOpIR condjmp = {0};
    condjmp.kind = kind;
    condjmp.data.jumpTo = label;
    BIRB_PushCopy(irbuf,&condjmp);
}

static void
BCCompileJump(BCIRBuffer *irbuf, ByteOpIR *label) {
    BCCompileJumpEx(irbuf,label,BOK_JUMP);
}

static void
BCCompileConditionalJump(BCIRBuffer *irbuf,AST *condition, bool ifNotZero, ByteOpIR *label, BCContext context) {
    ByteOpIR condjmp = {0};
    condjmp.data.jumpTo = label;
    if (!condition) {
        ERROR(NULL,"Null condition!");
        return;
    } else if (IsConstExpr(condition)) {
        int ival = EvalConstExpr(condition);
        if (!!ival == !!ifNotZero) {  
            condjmp.kind = BOK_JUMP; // Unconditional jump
        } else return; // Impossible jump
    } else if (condition->kind == AST_OPERATOR && condition->d.ival == K_BOOL_NOT) {
        // Inverted jump
        BCCompileConditionalJump(irbuf,condition->right,!ifNotZero,label,context);
        return;
    } else if (condition->kind == AST_OPERATOR && condition->d.ival == K_BOOL_AND && !ifNotZero) {
        // like in "IF L AND R"
        BCCompileConditionalJump(irbuf,condition->left,false,label,context);
        BCCompileConditionalJump(irbuf,condition->right,false,label,context);
        return;
    } else if (condition->kind == AST_OPERATOR && condition->d.ival == K_BOOL_AND && ifNotZero) {
        // like in "IFNOT L AND R"
        ByteOpIR *skipLabel = BCNewOrphanLabel();
        BCCompileConditionalJump(irbuf,condition->left,false,skipLabel,context);
        BCCompileConditionalJump(irbuf,condition->right,true,label,context);
        BIRB_Push(irbuf,skipLabel);
        return;
    } else if (condition->kind == AST_OPERATOR && condition->d.ival == K_BOOL_OR && !ifNotZero) {
        // like in "IF L OR R"
        ByteOpIR *skipLabel = BCNewOrphanLabel();
        BCCompileConditionalJump(irbuf,condition->left,true,skipLabel,context);
        BCCompileConditionalJump(irbuf,condition->right,false,label,context);
        BIRB_Push(irbuf,skipLabel);
        return;
    } else if (condition->kind == AST_OPERATOR && condition->d.ival == K_BOOL_OR && ifNotZero) {
        // like in "IFNOT L OR R"
        BCCompileConditionalJump(irbuf,condition->left,true,label,context);
        BCCompileConditionalJump(irbuf,condition->right,true,label,context);
        return;
    } else {
        // Just normal
        BCCompileExpression(irbuf,condition,context,false);
        condjmp.kind = ifNotZero ? BOK_JUMP_IF_NZ : BOK_JUMP_IF_Z;
    }
    BIRB_PushCopy(irbuf,&condjmp);
}

static void
BCCompileMemOpEx(BCIRBuffer *irbuf,AST *node,BCContext context, enum ByteOpKind kind, enum MathOpKind modifyMathKind, bool modifyReverseMath, bool pushModifyResult) {
    ByteOpIR memOp = {0};
    memOp.kind = kind;
    memOp.mathKind = modifyMathKind;
    memOp.attr.memop.modifyReverseMath = modifyReverseMath;
    memOp.attr.memop.pushModifyResult = pushModifyResult;

    AST *type = NULL;
    AST *typeoverride = NULL;
    Symbol *sym = NULL;
    AST *baseExpr = NULL;
    AST *indexExpr = NULL;

    printASTInfo(node);
    AST *ident;
    if (node->kind == AST_ARRAYREF) {
        ident = node->left;
        indexExpr = node->right;
    } else ident = node;

    try_ident_again:
    if (IsIdentifier(ident)) sym = LookupAstSymbol(ident,NULL);
    else if (ident->kind == AST_RESULT) sym = LookupSymbol("result");
    else if (ident->kind == AST_MEMREF) {
        if (!typeoverride && 
        ident->right && ident->right->kind == AST_ADDROF && 
        ident->right->left && ident->right->left->kind == AST_ARRAYREF) {
            // Handle weird AST representation of "var.byte[x]""
            NOTE(node,"handling size override...");
            typeoverride = ident->left;
            ident = ident->right->left->left;
            goto try_ident_again;
        } else {
            // normal raw memory access
            memOp.attr.memop.base = MEMOP_BASE_POP;
            type = ident->left;
            baseExpr = ident->right;
            goto nosymbol_memref;
        }
    }

    if (!sym) {
        ERROR(ident,"Can't get symbol");
        return;
    } else {
    printf("got symbol with name %s and kind %d\n",sym->our_name,sym->kind);
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
            printf("Got SYM_VARIABLE (VAR symbol)... ");
            if (!strcmp(sym->our_name,"__clkfreq_var") || !strcmp(sym->our_name,"__clkmode_var")) {
                // FIXME figure out how to properly differentiate these
                printf("Got special symbol %s with offset %d\n",sym->our_name,sym->offset);
                memOp.attr.memop.base = MEMOP_BASE_POP;
                if (baseExpr) ERROR(node,"baseExpr already set?!?!");
                else baseExpr = AstInteger(sym->offset);
            } else {
                memOp.attr.memop.base = MEMOP_BASE_VBASE;
                printf("sym->offset is %d\n",sym->offset);
                memOp.data.int32 = sym->offset;
            }

        } break;
        case SYM_LOCALVAR: {
            printf("Got SYM_LOCALVAR... ");
            memOp.attr.memop.base = MEMOP_BASE_DBASE;
            printf("sym->offset is %d\n",sym->offset);
            memOp.data.int32 = sym->offset + BCLocalBase();
        } break;
        case SYM_PARAMETER: {
            printf("Got SYM_PARAMETER... ");
            memOp.attr.memop.base = MEMOP_BASE_DBASE;
            printf("sym->offset is %d\n",sym->offset);
            memOp.data.int32 = sym->offset + BCParameterBase();
        } break;
        case SYM_RESERVED: {
            if (!strcmp(sym->our_name,"result")) {
                goto do_result;
            } else {
                ERROR(node,"Unhandled reserved word %s in assignment",sym->our_name);
            }
        }
        do_result:
        case SYM_RESULT: {
            printf("Got SYM_RESULT... ");
            memOp.attr.memop.base = MEMOP_BASE_DBASE;
            printf("sym->offset is %d\n",sym->offset);
            memOp.data.int32 = sym->offset + BCResultsBase();
        } break;
        default:
            ERROR(ident,"Unhandled Symbol type: %d",sym->kind);
            return;
        }
    }
    nosymbol_memref:
    
    if (typeoverride) type = typeoverride;
    printf("Got type ");printASTInfo(type);
    if (type && type->kind == AST_ARRAYTYPE) type = type->left; // We don't care if this is an array, we just want the size

         if (type == ast_type_byte) memOp.attr.memop.memSize = MEMOP_SIZE_BYTE;
    else if (type == ast_type_word) memOp.attr.memop.memSize = MEMOP_SIZE_WORD; 
    else if (type == ast_type_long) memOp.attr.memop.memSize = MEMOP_SIZE_LONG;
    else if (type == NULL) memOp.attr.memop.memSize = MEMOP_SIZE_LONG; // Assume long type... This is apparently neccessary
    else {
        ERROR(type,"Can't figure out mem op type, is unhandled");
        printASTInfo(type);
    }

    memOp.attr.memop.modSize = memOp.attr.memop.memSize; // Let's just assume these are the same

    if (!!baseExpr != !!(memOp.attr.memop.base == MEMOP_BASE_POP)) ERROR(node,"Internal Error: baseExpr condition mismatch");
    if (baseExpr) BCCompileExpression(irbuf,baseExpr,context,false);

    if (indexExpr) {
        bool indexConst = IsConstExpr(indexExpr);
        int constIndexVal;
        if (indexConst) constIndexVal = EvalConstExpr(indexExpr);

        if (baseExpr && indexConst && constIndexVal == 0) {
            // In this case, do nothing
        } else if (!baseExpr && indexConst) {
            // Just add index onto base
            if (memOp.attr.memop.memSize == MEMOP_SIZE_BYTE) memOp.data.int32 += constIndexVal;
            else if (memOp.attr.memop.memSize == MEMOP_SIZE_WORD) memOp.data.int32 += constIndexVal << 1;
            else if (memOp.attr.memop.memSize == MEMOP_SIZE_LONG) memOp.data.int32 += constIndexVal << 2;
        } else {
            // dynamic index
            memOp.attr.memop.popIndex = true;
            BCCompileExpression(irbuf,indexExpr,context,false);
        }
    }

    BIRB_PushCopy(irbuf,&memOp);
}

static inline void
BCCompileMemOp(BCIRBuffer *irbuf,AST *node,BCContext context, enum ByteOpKind kind) {
    BCCompileMemOpEx(irbuf,node,context,kind,0,false,false);
}

static void
BCCompileAssignment(BCIRBuffer *irbuf,AST *node,BCContext context,bool asExpression) {
    AST *left = node->left, *right = node->right;
    printf("Got an assign! left kind: %d, right kind %d\n",left->kind,right->kind);

    if (asExpression) ERROR(node,"Assignment expression NYI");  

    BCCompileExpression(irbuf,right,context,false);

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
        BCCompileMemOp(irbuf,left,context,BOK_MEM_WRITE);
    } break;
    case AST_RESULT: {
        printf("Got RESULT assign\n");
        BCCompileMemOp(irbuf,left,context,BOK_MEM_WRITE);
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
        case SYM_LOCALVAR:
        case SYM_PARAMETER:
        case SYM_RESULT:
            BCCompileMemOp(irbuf,left,context,BOK_MEM_WRITE);
            break;
        case SYM_RESERVED:
            if (!strcmp(sym->our_name,"result")) {
                // FIXME the parser should give AST_RESULT here, I think
                BCCompileMemOp(irbuf,NewAST(AST_RESULT,left->left,left->right),context,BOK_MEM_WRITE);
            } else {
                ERROR(node,"Unhandled reserved word %s in assignment",sym->our_name);
            }
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

static int getFuncID(Module *M,const char *name) {
    if (!M->bedata) {
        ERROR(NULL,"Internal Error: bedata empty");
        return -1;
    }
    for (int i=0;i<ModData(M)->pub_cnt;i++) {
        if (!strcmp(ModData(M)->pubs[i]->name,name)) return i + 1;
    }
    for (int i=0;i<ModData(M)->pri_cnt;i++) {
        if (!strcmp(ModData(M)->pris[i]->name,name)) return ModData(M)->pub_cnt + i + 1;
    }
    return -1;
}

static int getFuncIDForKnownFunc(Module *M,Function *F) {
    if (!M->bedata) {
        ERROR(NULL,"Internal Error: bedata empty");
        return -1;
    }
    for (int i=0;i<ModData(M)->pub_cnt;i++) {
        if (ModData(M)->pubs[i] == F) return i + 1;
    }
    for (int i=0;i<ModData(M)->pri_cnt;i++) {
        if (ModData(M)->pris[i] == F) return ModData(M)->pub_cnt + i + 1;
    }
    return -1;
}

// FIXME this seems very convoluted
static int getObjID(Module *M,const char *name, AST** gettype) {
    if (!M->bedata) {
        ERROR(NULL,"Internal Error: bedata empty");
        return -1;
    }
    for(int i=0;i<ModData(M)->obj_cnt;i++) {
        AST *obj = ModData(M)->objs[i];
        ASSERT_AST_KIND(obj,AST_DECLARE_VAR,return 0;);
        ASSERT_AST_KIND(obj->left,AST_OBJECT,return 0;);
        ASSERT_AST_KIND(obj->right,AST_LISTHOLDER,return 0;);
        AST *ident = obj->right->left;
        if (ident && ident->kind == AST_ARRAYDECL) ident = ident->left;
        ASSERT_AST_KIND(ident,AST_IDENTIFIER,return 0;);
        if(strcmp(GetIdentifierName(ident),name)) continue; 
        if (gettype) *gettype = obj->left;
        return i+1+ModData(M)->pub_cnt+ModData(M)->pri_cnt;
    }
    ERROR(NULL,"can't find OBJ id for %s",name);
    return 0;
}

// Very TODO
static void
BCCompileFunCall(BCIRBuffer *irbuf,AST *node,BCContext context, bool asExpression) {
    printf("Compiling fun call, ");
    printASTInfo(node);

    Symbol *sym = NULL;
    int callobjid = -1;
    AST *objtype = NULL;
    AST *index_expr = NULL;
    if (node->left && node->left->kind == AST_METHODREF) {
        AST *ident = node->left->left;
        if (ident->kind == AST_ARRAYREF) {
            index_expr = ident->right;
            ident = ident->left;
        }
        ASSERT_AST_KIND(ident,AST_IDENTIFIER,return;);
        Symbol *objsym = LookupAstSymbol(ident,NULL);
        if(!objsym || objsym->kind != SYM_VARIABLE) {
            ERROR(node->left,"Internal error: Invalid call receiver");
            return;
        }
        printf("Got object with name %s\n",objsym->our_name);

        callobjid = getObjID(current,objsym->our_name,&objtype);
        if (callobjid<0) {
            ERROR(node->left,"Not an OBJ of this object");
            return;
        }

        const char *thename = GetIdentifierName(node->left->right);
        if (!thename) {
            return;
        }
        
        sym = LookupMemberSymbol(node, objtype, thename, NULL);
        if (!sym) {
            ERROR(node->left, "%s is not a member", thename);
            return;
        }
    } else {
        sym = LookupAstSymbol(node->left, NULL);
    }

    ByteOpIR callOp = {0};
    ByteOpIR anchorOp = {0};
    anchorOp.kind = BOK_ANCHOR;
    anchorOp.attr.anchor.withResult = asExpression;
    if (!sym) {
        ERROR(node,"Function call has no symbol");
        return;
    } else if (sym->kind == SYM_BUILTIN) {
        anchorOp.kind = 0; // Where we're going, we don't need Stack Frames
        if (asExpression) {
            if (!strcmp(sym->our_name,"__builtin_strlen")) {
                callOp.kind = BOK_BUILTIN_STRSIZE;
            } else if (!strcmp(sym->our_name,"strcomp")) {
                callOp.kind = BOK_BUILTIN_STRCOMP;
            } else {
                ERROR(node,"Unhandled expression builtin %s",sym->our_name);
                return;
            }
        } else {
            if (!strcmp(sym->our_name,"waitcnt")) {
                callOp.kind = BOK_WAIT;
                callOp.attr.wait.type = BCW_WAITCNT;
            } else if (!strcmp(sym->our_name,"waitpeq")) {
                callOp.kind = BOK_WAIT;
                callOp.attr.wait.type = BCW_WAITPEQ;
            } else if (!strcmp(sym->our_name,"waitpne")) {
                callOp.kind = BOK_WAIT;
                callOp.attr.wait.type = BCW_WAITPNE;
            } else if (!strcmp(sym->our_name,"waitvid")) {
                callOp.kind = BOK_WAIT;
                callOp.attr.wait.type = BCW_WAITVID;
            } else if (!strcmp(sym->our_name,"bytefill")) {
                callOp.kind = BOK_BUILTIN_BULKMEM;
                callOp.attr.bulkmem.isMove = false;
                callOp.attr.bulkmem.memSize = BULKMEM_SIZE_BYTE;
            } else if (!strcmp(sym->our_name,"wordfill")) {
                callOp.kind = BOK_BUILTIN_BULKMEM;
                callOp.attr.bulkmem.isMove = false;
                callOp.attr.bulkmem.memSize = BULKMEM_SIZE_WORD;
            } else if (!strcmp(sym->our_name,"longfill")) {
                callOp.kind = BOK_BUILTIN_BULKMEM;
                callOp.attr.bulkmem.isMove = false;
                callOp.attr.bulkmem.memSize = BULKMEM_SIZE_LONG;
            } else if (!strcmp(sym->our_name,"bytemove")) {
                callOp.kind = BOK_BUILTIN_BULKMEM;
                callOp.attr.bulkmem.isMove = true;
                callOp.attr.bulkmem.memSize = BULKMEM_SIZE_BYTE;
            } else if (!strcmp(sym->our_name,"wordmove")) {
                callOp.kind = BOK_BUILTIN_BULKMEM;
                callOp.attr.bulkmem.isMove = true;
                callOp.attr.bulkmem.memSize = BULKMEM_SIZE_WORD;
            } else if (!strcmp(sym->our_name,"longmove")) {
                callOp.kind = BOK_BUILTIN_BULKMEM;
                callOp.attr.bulkmem.isMove = true;
                callOp.attr.bulkmem.memSize = BULKMEM_SIZE_LONG;
            } else if (!strcmp(sym->our_name,"_clkset") && !gl_p2) {
                callOp.kind = BOK_CLKSET;
            } else if (!strcmp(sym->our_name,"_reboot")) {
                if (gl_p2) ERROR(node,"P2 REBOOT is NYI");
                else {
                    printf("Got reboot!\n");
                    callOp.kind = BOK_CLKSET;
                    // Slight Hack: compile the parameters up here
                    BCCompileInteger(irbuf,128);
                    BCCompileInteger(irbuf,0);
                }
            } else if (!strcmp(sym->our_name,"_cogstop")) {
                callOp.kind = BOK_COGSTOP;
            } else {
                ERROR(node,"Unhandled statement builtin %s",sym->our_name);
                return;
            }
        }
    } else if (sym->kind == SYM_FUNCTION) {
        // Function call
        int funid = getFuncID(objtype ? GetClassPtr(objtype) : current, sym->our_name);
        if (funid < 0) {
            ERROR(node,"Can't get function id for %s",sym->our_name);
            return;
        }
        printf("Got call to %s\n",sym->our_name);
        if (callobjid<0) {
            callOp.kind = BOK_CALL_SELF;
        } else {
            callOp.kind = index_expr ? BOK_CALL_OTHER_IDX : BOK_CALL_OTHER;
            callOp.attr.call.objID = callobjid;
        }
        callOp.attr.call.funID = funid;

    } else {
        ERROR(node,"Unhandled FUNCALL symbol (name is %s and sym kind is %d)",sym->our_name,sym->kind);
        return;
    }

    if (anchorOp.kind) BIRB_PushCopy(irbuf,&anchorOp);

    if (node->right) {
        ASSERT_AST_KIND(node->right,AST_EXPRLIST,return;);
        for (AST *list=node->right;list;list=list->right) {
            printf("Compiling function call argument...");
            ASSERT_AST_KIND(list,AST_EXPRLIST,return;);
            BCCompileExpression(irbuf,list->left,context,false);
        }
    }

    if (index_expr) BCCompileExpression(irbuf,index_expr,context,false);
    BIRB_PushCopy(irbuf,&callOp);

    // TODO pop unwanted results????

}

static void
BCCompileCoginit(BCIRBuffer *irbuf,AST *node,BCContext context,bool asExpression) {
    Function *calledmethod;
    if (IsSpinCoginit(node,&calledmethod)) {
        // Spin coginit
        printf("Got Spin coginit with function %s\n",calledmethod->name);
        if(gl_interp_kind != INTERP_KIND_P1ROM) {
            ERROR(node,"Spin coginit NYI for this interpreter");
            return;
        }

        ASSERT_AST_KIND(node->left,AST_EXPRLIST,return;);
        AST *cogidExpr = node->left->left;

        if (node->left->right->left && node->left->right->left->kind == AST_FUNCCALL) {
            // Arguments
            AST *funcall = node->left->right->left;
            for (AST *list=funcall->right;list;list=list->right) {
                printf("Compiling coginit call argument...");
                ASSERT_AST_KIND(list,AST_EXPRLIST,return;);
                BCCompileExpression(irbuf,list->left,context,false);
            }

        }

        // Target function info
        if (calledmethod->module != current) {
            ERROR(node,"Interpreter can't coginit with method from another object");
            return;
        }
        BCCompileInteger(irbuf, (calledmethod->numparams << 8) + getFuncIDForKnownFunc(current,calledmethod));

        ASSERT_AST_KIND(node->left->right->right,AST_EXPRLIST,return;); 
        BCCompileExpression(irbuf,node->left->right->right->left,context,false); // New Stack pointer

        ByteOpIR prepareOP = {.kind = BOK_COGINIT_PREPARE};
        BIRB_PushCopy(irbuf,&prepareOP);

        if (IsConstExpr(cogidExpr)) {
            uint32_t val = EvalConstExpr(cogidExpr);
            // Parser gives 0x1E for COGNEW, but -1 is smaller, thus this
            if (!gl_p2 && (val & 8)) BCCompileInteger(irbuf,-1);
            else BCCompileInteger(irbuf,val);
        } else {
            BCCompileExpression(irbuf,cogidExpr,context,false);
        }
        ByteOpIR dcurrOp = {.kind = BOK_REG_READ,.data.int32=15}; // This reads DCURR (OUR stack pointer!)
        BIRB_PushCopy(irbuf,&dcurrOp);
        BCCompileInteger(irbuf,-4); // Magic number
        ByteOpIR magicWriteOp = {.kind = BOK_MEM_WRITE,.attr.memop = {.base = MEMOP_BASE_POP, .memSize = MEMOP_SIZE_LONG, .popIndex = true}};
        BIRB_PushCopy(irbuf,&magicWriteOp);  // Write cogid????



    } else {
        // PASM coginit
        printf("Got PASM coginit\n");

        ASSERT_AST_KIND(node->left,AST_EXPRLIST,return;);
        AST *cogidExpr = node->left->left;
        if (IsConstExpr(cogidExpr)) {
            uint32_t val = EvalConstExpr(cogidExpr);
            // Parser gives 0x1E for COGNEW, but -1 is smaller, thus this
            if (!gl_p2 && (val & 8)) BCCompileInteger(irbuf,-1);
            else BCCompileInteger(irbuf,val);
        } else {
            BCCompileExpression(irbuf,cogidExpr,context,false);
        }

        ASSERT_AST_KIND(node->left->right,AST_EXPRLIST,return;);
        BCCompileExpression(irbuf,node->left->right->left,context,false); // entry point

        ASSERT_AST_KIND(node->left->right->right,AST_EXPRLIST,return;);
        BCCompileExpression(irbuf,node->left->right->right->left,context,false); // PAR value
    }

    ByteOpIR initOp = {.kind = BOK_COGINIT};
    initOp.attr.coginit.pushCogID = asExpression;
    BIRB_PushCopy(irbuf,&initOp);
}

static void
BCCompileExpression(BCIRBuffer *irbuf,AST *node,BCContext context,bool asStatement) {
    if(!node) {
        ERROR(NULL,"Internal Error: Null expression!!");
        return;
    }
    if (IsConstExpr(node)) {
        int32_t val = EvalConstExpr(node);
        printf("Got const expression with value %d\n",val);
        BCCompileInteger(irbuf,val);
    } else {
        unsigned popResults = asStatement ? 1 : 0;
        switch(node->kind) {
            case AST_FUNCCALL: {
                printf("Got call in expression \n");
                BCCompileFunCall(irbuf,node,context,true);
            } break;
            case AST_ASSIGN: {
                printf("Got assignment in expression \n");
                BCCompileAssignment(irbuf,node,context,true);
            } break;
            case AST_OPERATOR: {
                printf("Got operator in expression: 0x%03X\n",node->d.ival);
                printASTInfo(node);
                enum MathOpKind mok;
                bool unary = false;
                AST *left = node->left,*right = node->right;
                switch(node->d.ival) {
                case '^': mok = MOK_BITXOR; break;
                case '|': mok = MOK_BITOR; break;
                case '&': mok = MOK_BITAND; break;
                case '+': mok = MOK_ADD; break;
                case '-': mok = MOK_SUB; break;
                case '*': mok = MOK_MULLOW; break;
                case K_HIGHMULT: mok = MOK_MULHIGH; break;
                case '/': mok = MOK_DIVIDE; break;
                case K_MODULUS: mok = MOK_REMAINDER; break;
                case K_LIMITMAX: mok = MOK_MAX; break;
                case K_LIMITMIN: mok = MOK_MIN; break;


                case K_SHR: mok = MOK_SHR; break;
                case K_SAR: mok = MOK_SAR; break;
                case K_ROTL: mok = MOK_ROL; break;
                case K_ROTR: mok = MOK_ROR; break;

                case K_REV: mok = MOK_REV; break;

                case K_SHL: {
                    if (IsConstExpr(left) && EvalConstExpr(left)==1) {
                        // convert 1<<X into decode
                        mok = MOK_DECODE;
                        unary = true;
                        left = NULL;
                    } else {
                        mok = MOK_SHL; 
                    }
                } break;

                case '<': mok = MOK_CMP_B; break;
                case '>': mok = MOK_CMP_A; break;
                case K_LE: mok = MOK_CMP_BE; break;
                case K_GE: mok = MOK_CMP_AE; break;
                case K_EQ: mok = MOK_CMP_E; break;
                case K_NE: mok = MOK_CMP_NE; break;

                case K_BOOL_NOT: mok = MOK_BOOLNOT; unary=true; break;
                case K_NEGATE: mok = MOK_NEG; unary = true; break;
                case K_ABS: mok = MOK_ABS; unary = true; break;
                case K_SQRT: mok = MOK_SQRT; unary = true; break;
                case K_DECODE: mok = MOK_DECODE; unary = true; break;
                case K_ENCODE: mok = MOK_ENCODE; unary = true; break;

                case K_LOGIC_AND: mok = MOK_LOGICAND; break;
                case K_LOGIC_OR: mok = MOK_LOGICOR; break;

                case K_BOOL_AND:
                case K_BOOL_OR: {
                    bool is_and = node->d.ival == K_BOOL_AND;
                    if (!ExprHasSideEffects(right)) {
                        // Use logic instead
                        mok = is_and ? MOK_LOGICAND : MOK_LOGICOR;
                        break;
                    } else {
                        ByteOpIR notOp = {.kind = BOK_MATHOP,.mathKind = MOK_BOOLNOT};
                        // build rickety short circuit op
                        ByteOpIR *lbl = BCNewOrphanLabel();
                        BCCompileInteger(irbuf,is_and ? 0 : -1);
                        BCCompileConditionalJump(irbuf,left,!is_and,lbl,context);
                        BCCompileConditionalJump(irbuf,right,!is_and,lbl,context);
                        BIRB_PushCopy(irbuf,&notOp);
                        BIRB_Push(irbuf,lbl);
                        return;
                    }
                } break;

                case K_INCREMENT: 
                case K_DECREMENT:
                    goto incdec;
                default:
                    ERROR(node,"Unhandled operator 0x%03X",node->d.ival);
                    return;
                }

                printf("Operator resolved to %d = %s\n",mok,mathOpKindNames[mok]);

                if (!unary) BCCompileExpression(irbuf,left,context,false);
                BCCompileExpression(irbuf,right,context,false);

                ByteOpIR mathOp = {0};
                mathOp.kind = BOK_MATHOP;
                mathOp.mathKind = mok;
                BIRB_PushCopy(irbuf,&mathOp);

            } break;
            incdec: { // Special handling for inc/dec
                bool isPostfix = node->left;
                AST *var = isPostfix?node->left:node->right;
                bool isDecrement = node->d.ival == K_DECREMENT;

                if (asStatement) popResults = 0;

                if (0) {
                    // Native inc/dec
                    enum MathOpKind mok = isDecrement ? (isPostfix ? MOK_MOD_POSTDEC : MOK_MOD_PREDEC) : (isPostfix ? MOK_MOD_POSTINC : MOK_MOD_PREINC);
                    BCCompileMemOpEx(irbuf,var,context,BOK_MEM_MODIFY,mok,false,!asStatement);
                } else {
                    // Discrete inc/dec

                    // If postfix, push old value
                    if (isPostfix && !asStatement) BCCompileMemOp(irbuf,var,context,BOK_MEM_READ);

                    BCCompileMemOp(irbuf,var,context,BOK_MEM_READ);
                    BCCompileInteger(irbuf,1);
                    ByteOpIR incdecop = {0};
                    incdecop.kind = BOK_MATHOP;
                    incdecop.mathKind = isDecrement ? MOK_SUB : MOK_ADD;
                    BIRB_PushCopy(irbuf,&incdecop);
                    BCCompileMemOp(irbuf,var,context,BOK_MEM_WRITE);

                    // If prefix, push new value
                    if (!isPostfix && !asStatement) BCCompileMemOp(irbuf,var,context,BOK_MEM_READ);
                }

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
                case SYM_LOCALVAR:
                case SYM_PARAMETER:
                case SYM_RESULT:
                    BCCompileMemOp(irbuf,node,context,BOK_MEM_READ);
                    break;
                case SYM_RESERVED:
                    if (!strcmp(sym->our_name,"result")) {
                        // FIXME the parser should give AST_RESULT here, I think
                        BCCompileMemOp(irbuf,NewAST(AST_RESULT,node->left,node->right),context,BOK_MEM_READ);
                    } else {
                        ERROR(node,"Unhandled reserved word %s in expression",sym->our_name);
                    }
                    break;
                default:
                    ERROR(node,"Unhandled Identifier symbol kind %d",sym->kind);
                    return;
                }

            } break;
            case AST_RESULT: {
                printf("Got RESULT!\n");
                BCCompileMemOp(irbuf,node,context,BOK_MEM_READ);
            } break;
            case AST_ARRAYREF: {
                printf("Got array read! ");
                printASTInfo(node);
                BCCompileMemOp(irbuf,node,context,BOK_MEM_READ);
            } break;
            case AST_STRINGPTR: {
                printf("Got STRINGPTR! ");
                printASTInfo(node);
                ByteOpIR *stringLabel = BCNewOrphanLabel();
                ByteOpIR pushOp = {0};
                pushOp.kind = BOK_FUNDATA_PUSHADDRESS;
                pushOp.data.jumpTo = stringLabel;

                ByteOpIR stringData = BCBuildString(node->left);

                BIRB_PushCopy(irbuf,&pushOp);
                BIRB_Push(irbuf->pending,stringLabel);
                BIRB_PushCopy(irbuf->pending,&stringData);

            } break;
            default:
                ERROR(node,"Unhandled node kind %d in expression",node->kind);
                return;
        }
        if (popResults) {
            BCCompileInteger(irbuf,popResults);
            ByteOpIR popOp = {0};
            popOp.kind = BOK_POP;
            BIRB_PushCopy(irbuf,&popOp);
        }
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
        BCCompileStatement(irbuf,node,context);
    }

}

static void
BCCompileStatement(BCIRBuffer *irbuf,AST *node, BCContext context) {
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
        BCCompileConditionalJump(irbuf,node->left,false,bottomlbl,context);

        ASSERT_AST_KIND(node->right,AST_STMTLIST,break;)

        BCContext newcontext = context;
        newcontext.nextLabel = toplbl;
        newcontext.quitLabel = bottomlbl;
        BCCompileStmtlist(irbuf,node->right,newcontext);

        BCCompileJump(irbuf,toplbl);
        BIRB_Push(irbuf,bottomlbl);
    } break;
    case AST_DOWHILE: {
        printf("Got DoWhile loop.. ");
        printASTInfo(node);

        ByteOpIR *toplbl = BCPushLabel(irbuf);
        ByteOpIR *nextlbl = BCNewOrphanLabel();
        ByteOpIR *bottomlbl = BCNewOrphanLabel();

        BCContext newcontext = context;
        newcontext.nextLabel = nextlbl;
        newcontext.quitLabel = bottomlbl;
        ASSERT_AST_KIND(node->right,AST_STMTLIST,break;)
        BCCompileStmtlist(irbuf,node->right,newcontext);

        BIRB_Push(irbuf,nextlbl);
        // Compile condition
        BCCompileConditionalJump(irbuf,node->left,true,toplbl,context);

        BIRB_Push(irbuf,bottomlbl);
    } break;
    case AST_FOR:
    case AST_FORATLEASTONCE: {
        printf("Got For... ");
        printASTInfo(node);

        bool atleastonce = node->kind == AST_FORATLEASTONCE;

        ByteOpIR *topLabel = BCNewOrphanLabel();
        ByteOpIR *nextLabel = BCNewOrphanLabel();
        ByteOpIR *quitLabel = BCNewOrphanLabel();

        AST *initStmnt = node->left;
        AST *to = node->right;
        ASSERT_AST_KIND(to,AST_TO,return;);
        printf("to: ");printASTInfo(to);
        AST *condExpression = to->left;
        AST *step = to->right;
        ASSERT_AST_KIND(step,AST_STEP,return;);
        printf("step: ");printASTInfo(step);
        AST *nextExpression = step->left;
        AST *body = step->right;
        ASSERT_AST_KIND(body,AST_STMTLIST,return;);

        BCCompileStatement(irbuf,initStmnt,context);
        BIRB_Push(irbuf,topLabel);
        if(!atleastonce) BCCompileConditionalJump(irbuf,condExpression,false,quitLabel,context);

        BCContext newcontext = context;
        newcontext.quitLabel = quitLabel;
        newcontext.nextLabel = nextLabel;
        BCCompileStmtlist(irbuf,body,newcontext);

        BIRB_Push(irbuf,nextLabel);
        BCCompileExpression(irbuf,nextExpression,context,true); // Compile as statement!

        if(atleastonce) BCCompileConditionalJump(irbuf,condExpression,true,topLabel,context);
        else BCCompileJump(irbuf,topLabel);

        BIRB_Push(irbuf,quitLabel);

    } break;
    case AST_IF: {
        printf("Got If... ");
        printASTInfo(node);

        ByteOpIR *bottomlbl = BCNewOrphanLabel();
        ByteOpIR *elselbl = BCNewOrphanLabel();

        AST *thenelse = node->right;
        if (thenelse && thenelse->kind == AST_COMMENTEDNODE) thenelse = thenelse->left;
        ASSERT_AST_KIND(thenelse,AST_THENELSE,break;);

        // Compile condition
        BCCompileConditionalJump(irbuf,node->left,false,elselbl,context);

        ByteOpIR elsejmp = {0};
        elsejmp.kind = BOK_JUMP;
        elsejmp.data.jumpTo = bottomlbl;

        if(thenelse->left) { // Then
            ASSERT_AST_KIND(thenelse->left,AST_STMTLIST,break;);
            BCContext newcontext = context;
            BCCompileStmtlist(irbuf,thenelse->left,newcontext);
        }
        if(thenelse->right) BIRB_PushCopy(irbuf,&elsejmp);
        BIRB_Push(irbuf,elselbl);
        if(thenelse->right) { // Else
            ASSERT_AST_KIND(thenelse->right,AST_STMTLIST,break;);
            BCContext newcontext = context;
            BCCompileStmtlist(irbuf,thenelse->right,newcontext);
        }
        if(thenelse->right) BIRB_Push(irbuf,bottomlbl);
    } break;
    case AST_FUNCCALL: {
        BCCompileFunCall(irbuf,node,context,false);
    } break;
    case AST_CONTINUE: {
        if (!context.nextLabel) {
            ERROR(node,"NEXT outside loop");
        } else {
            ByteOpIR jumpOp = {0};
            jumpOp.kind = BOK_JUMP;
            jumpOp.data.jumpTo = context.nextLabel;
            BIRB_PushCopy(irbuf,&jumpOp);
        }
    } break;
    case AST_QUITLOOP: {
        if (!context.nextLabel) {
            ERROR(node,"QUIT outside loop");
        } else if (context.inCountedRepeat) {
            ERROR(node,"QUIT in counted repeat NYI");
        } else {
            ByteOpIR jumpOp = {0};
            jumpOp.kind = BOK_JUMP;
            jumpOp.data.jumpTo = context.quitLabel;
            BIRB_PushCopy(irbuf,&jumpOp);
        }
    } break;
    case AST_IDENTIFIER: {
        printf("Got identifier in Statement??\n");
        Symbol *sym = LookupAstSymbol(node,NULL);
        if (!sym) ERROR(node,"Internal Error: Can't get symbol");
        switch (sym->kind) {
        case SYM_BUILTIN:
        case SYM_FUNCTION: {
            // This nonsense fixes REBOOT
            NOTE(node,"identifier converted to function call for sym with kind %d and name %s",sym->kind,sym->our_name);
            AST *fakecall = NewAST(AST_FUNCCALL,node,NULL);
            BCCompileFunCall(irbuf,fakecall,context,false);
        } break;
        default: {
            ERROR(node,"Unhandled identifier symbol kind %d in statement",sym->kind);
        } break;
        }
    } break;
    case AST_COGINIT: {
        printf("got coginit statement! "); printASTInfo(node);
        BCCompileCoginit(irbuf,node,context,false);
    } break;
    default:
        ERROR(node,"Unhandled node kind %d in statement",node->kind);
        break;
    }
}


static void 
BCCompileFunction(ByteOutputBuffer *bob,Function *F) {

    printf("Compiling bytecode for function %s\n",F->name);
    curfunc = F; // Set this global, I guess;

    // first up, we now know the function's address, so let's fix it up in the header
    int func_ptr = bob->total_size;
    int func_offset = 0;
    int func_localsize = F->numparams*4 + F->numlocals*4; // SUPER DUPER FIXME smaller locals
    if (interp_can_multireturn() && F->numresults > 1) {
        ERROR(F->body,"Multi-return is not supported by the used interpreter");
        return;
    }
    Module *M = F->module;
    if (M->bedata == NULL || ModData(M)->compiledAddress < 0) {
        ERROR(NULL,"Compiling function on Module whose address is not yet determined???");
    } else func_offset = func_ptr - ModData(M)->compiledAddress;

    if(F->bedata == NULL || !FunData(F)->headerEntry) {
        ERROR(NULL,"Compiling function that does not have a header entry");
        return;
    }
    
    FunData(F)->compiledAddress = func_ptr;
    FunData(F)->localSize = func_localsize;

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
    BCIRBuffer irbuf_pending = {0};
    irbuf.pending = &irbuf_pending;


    // I think there's other body types so let's leave this instead of using ASSERT_AST_KIND
    if (!F->body) {
        NOTE(NULL,"compiling function with no body...");
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

    BIRB_AppendPending(&irbuf);

    BCIR_to_BOB(&irbuf,bob,func_ptr-ModData(current)->compiledAddress);
    
    curfunc = NULL;
}

static void
BCPrepareObject(Module *P) {
    // Init bedata
    if (P->bedata) return;

    printf("Preparing object %s\n",P->fullname);

    P->bedata = calloc(sizeof(BCModData), 1);
    ModData(P)->compiledAddress = -1;

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

    // Count and gather object members - (TODO does this actually work properly?)
    {
        int obj_cnt = 0;
        for (AST *upper = P->finalvarblock; upper; upper = upper->right) {
            if (upper->kind != AST_LISTHOLDER) {
                ERROR(upper, "internal error: expected listholder");
                return;
            }
            AST *var = upper->left;
            if (!(var->kind == AST_DECLARE_VAR && IsClassType(var->left))) continue;

            // Wrangle info from the AST
            ASSERT_AST_KIND(var->left,AST_OBJECT,return;);
            ASSERT_AST_KIND(var->right,AST_LISTHOLDER,return;);

            int arrsize = 0;
            if (!var->right->left) {
                ERROR(var->right,"LISTHOLDER is empty");
            } else if (var->right->left->kind == AST_IDENTIFIER) {
                printf("Got obj of type %s named %s\n",((Module*)var->left->d.ptr)->classname,var->right->left->d.string);
                arrsize = 1;
            } else if (var->right->left->kind == AST_ARRAYDECL) {
                ASSERT_AST_KIND(var->right->left->right,AST_INTEGER,return;);
                printf("Got obj array of type %s, size %d named %s\n",((Module*)var->left->d.ptr)->classname,var->right->left->right->d.ival,var->right->left->left->d.string);
                arrsize = var->right->left->right->d.ival;
            } else {
                ERROR(var->right,"Unhandled OBJ AST kind %d",var->right->left->kind);
            }
            //printASTInfo(var->right->left);
            //ASSERT_AST_KIND(var->right->left,AST_IDENTIFIER,return;);

            BCPrepareObject(GetClassPtr(var->left));

            for(int i=0;i<arrsize;i++) {
                ModData(P)->objs[obj_cnt] = var; // Just insert the same var and differentiate by objs_arr_index
                ModData(P)->objs_arr_index[obj_cnt] = i;
                obj_cnt++;
            }


            if (pub_cnt+pri_cnt+obj_cnt >= BC_MAX_POINTERS) {
                ERROR(NULL,"Too many objects in Module %s",P->fullname);
                return;
            }
        }
        ModData(P)->obj_cnt = obj_cnt;
    }
    
}

static void
BCCompileObject(ByteOutputBuffer *bob, Module *P) {

    printf("Compiling bytecode for object %s\n",P->fullname);

    BOB_Align(bob,4); // Long-align

    BOB_Comment(bob,auto_printf(128,"--- Object Header for %s",P->classname));

    BCPrepareObject(P);

    if (ModData(P)->compiledAddress < 0) {
        ModData(P)->compiledAddress = bob->total_size;
    } else {
        ERROR(NULL,"Already compiled module %s",P->fullname);
        return;
    }

    
    const int pub_cnt = ModData(P)->pub_cnt;
    const int pri_cnt = ModData(P)->pri_cnt;    
    const int obj_cnt = ModData(P)->obj_cnt;

    printf("Debug: this object (%s) has %d PUBs, %d PRIs and %d OBJs\n",P->classname,pub_cnt,pri_cnt,obj_cnt);

    OutputSpan *objOffsetSpans[obj_cnt];
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
            objOffsetSpans[i] = BOB_PushWord(bob,0,"Header offset");
            AST *obj = ModData(P)->objs[i];
            int varOffset = BCGetOBJOffset(P,obj) + ModData(P)->objs_arr_index[i] * BCGetOBJSize(P,obj);
            BOB_PushWord(bob,varOffset,"VAR offset"); // VAR Offset (from current object's VAR base)
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
        // FIXME for some reason, this sometimes(??) prints empty DAT blocks for subobjects ???
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
        if (mod->bedata == NULL || ModData(mod)->compiledAddress < 0) {
            BCCompileObject(bob,mod);
        }
        switch(gl_interp_kind) {
        case INTERP_KIND_P1ROM:
            BOB_ReplaceWord(objOffsetSpans[i],ModData(mod)->compiledAddress - ModData(P)->compiledAddress,NULL);
            break;
        default:
            ERROR(NULL,"Unknown interpreter kind");
            return;
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
    const int variableSize = P->varsize; // Already rounded up!
    const int stackBase = programSize + variableSize + 8; // space for that stack init thing

    printf("Program size:  %6d bytes\nVariable size: %6d bytes\nStack/Free:    %6d TODO\n",programSize,variableSize,0);


    // Fixup header
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        // TODO fixup everything
        BOB_ReplaceWord(headerspans.pcurr,FunData(ModData(P)->pubs[0])->compiledAddress,NULL);
        BOB_ReplaceWord(headerspans.vbase,programSize,NULL);
        BOB_ReplaceWord(headerspans.dbase,stackBase,NULL);
        BOB_ReplaceWord(headerspans.dcurr,stackBase+FunData(ModData(P)->pubs[0])->localSize,NULL);
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
