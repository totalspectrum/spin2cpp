//
// Bytecode compiler for spin2cpp
//
// Copyright 2021 Ada Gottensträter and Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//

#include "outbc.h"
#include "bcbuffers.h"
#include "bcir.h"
#include <stdlib.h>

const BCContext nullcontext = {.hiddenVariables = 0};

bool interp_can_multireturn() {
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
    } else ERROR(ast,"Unhandled AST Kind %d in BCGetOBJSize\n",ast->kind);
    return 0;
}

static int BCGetOBJOffset(Module *P,AST *ast) {
    Symbol *sym = NULL;
    if (ast->kind == AST_LISTHOLDER) ast = ast->right;
    if (ast->kind == AST_DECLARE_VAR) {
        // FIXME this seems kindof wrong?
        AST *ident = ast->right->left;
        if (ident->kind == AST_ARRAYDECL) ident = ident->left;
        if (ident->kind != AST_IDENTIFIER) {
            ERROR(ident,"Not an identifier");
            return 0;
        }
        const char *name = ident->d.string;
        if(name) sym = LookupSymbolInTable(&P->objsyms,name);
    } else if (ast->kind == AST_IDENTIFIER) {
        sym = LookupAstSymbol(ast,NULL);
    } else ERROR(ast,"Unhandled AST Kind %d in BCGetOBJOffset\n",ast->kind);
    if (!sym) {
        ERROR(ast,"Can't find symbol for an OBJ");
        return -1;
    }
    //printf("In BCGetOBJOffset: ");
    //printf("got symbol with name %s, kind %d, offset %d\n",sym->our_name,sym->kind,sym->offset);
    return sym->offset;
}

// Get offset from PBASE to DAT start
int BCgetDAToffset(Module *P, bool absolute, AST *errloc, bool printErrors) {
    if (gl_output != OUTPUT_BYTECODE) {
        if (printErrors) ERROR(errloc,"BCgetDAToffset called, but not in bytecode mode");
        return -1;
    }
    if (!P->bedata) {
        if (printErrors) ERROR(errloc,"BCgetDAToffset: bedata for module %s uninitialized",P->classname);
        return -1;
    }
    int pbase_offset = -1;
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM: pbase_offset = 4*(ModData(current)->pub_cnt+ModData(current)->pri_cnt+ModData(current)->obj_cnt+1); break;
    default:
        if (printErrors) ERROR(errloc,"Unknown interpreter kind");
        return -1;
    }
    if (absolute) {
        int compiledAddress = ModData(P)->compiledAddress;
        if (compiledAddress < 0) {
            if (printErrors) ERROR(errloc,"Internal error: Taking address of uncompiled module");
            return -1;
        }
        return pbase_offset + compiledAddress;
    } else return pbase_offset;
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

static int
HWReg2Index(HwReg *reg) {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM: {
        int index = reg->addr - 0x1E0;
        if (index < 0 || index >= 32) {
            ERROR(NULL,"Register index %d for %s out of range",index,reg->name);
            return 0;
        }
        return index;
    } break;
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return 0;
    }
}

// Get math op for AST operator token
// Returns 0 if it is not a simple binary operator
static enum MathOpKind
Optoken2MathOpKind(int token,bool *unaryOut) {
    bool unary = false;
    enum MathOpKind mok = 0;
    //printf("In Optoken2MathOpKind, optoken %03X\n",token);
    switch (token) {
    default: return 0;
    
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

    case '<': mok = MOK_CMP_B; break;
    case '>': mok = MOK_CMP_A; break;
    case K_LE: mok = MOK_CMP_BE; break;
    case K_GE: mok = MOK_CMP_AE; break;
    case K_EQ: mok = MOK_CMP_E; break;
    case K_NE: mok = MOK_CMP_NE; break;

    case K_SHL: mok = MOK_SHL; break;
    case K_SHR: mok = MOK_SHR; break;
    case K_SAR: mok = MOK_SAR; break;
    case K_ROTL: mok = MOK_ROL; break;
    case K_ROTR: mok = MOK_ROR; break;

    case K_REV: mok = MOK_REV; break;

    case K_LOGIC_AND: mok = MOK_LOGICAND; break;
    case K_LOGIC_OR: mok = MOK_LOGICOR; break;


    case K_BOOL_NOT: mok = MOK_BOOLNOT; unary=true; break;
    case K_NEGATE: mok = MOK_NEG; unary = true; break;
    case K_ABS: mok = MOK_ABS; unary = true; break;
    case K_SQRT: mok = MOK_SQRT; unary = true; break;
    case K_DECODE: mok = MOK_DECODE; unary = true; break;
    case K_ENCODE: mok = MOK_ENCODE; unary = true; break;
    case K_BIT_NOT: mok = MOK_BITNOT; unary = true; break;

    }

    if (unaryOut) *unaryOut = unary;
    return mok;
}

static void OptimizeOperator(int *optoken, AST **left,AST **right) {
    ASTReportInfo save;

    // Try special cases first
    if (*optoken == K_SHL && left && IsConstExpr(*left) && EvalConstExpr(*left) == 1) {
        *left = NULL;
        *optoken = K_DECODE;
        return;
    }

    int shiftOptOp = 0;
    bool canNopOpt = false;
    int32_t nopOptVal;
    bool canZeroOpt = false;
    int32_t zeroOptVal;
    bool canCommute = false;

    switch (*optoken) {
    case '*':
        canCommute = true;
        shiftOptOp = K_SHL;
        canNopOpt = true;
        nopOptVal = 1;
        canZeroOpt = true;
        zeroOptVal = 0;
        break;
#if 0 /* fails for negative numbers */
    case '/':
        shiftOptOp = K_SAR;
        canNopOpt = true;
        nopOptVal = 1;
        break;
#endif        
    case K_SHL:
    case K_SHR:
    case K_SAR:
    case K_ROTL:
    case K_ROTR:
        canNopOpt = true;
        nopOptVal = 0;
        break;
    case '-':
        canNopOpt = true;
        nopOptVal = 0;
        break;
    case '+':
        canCommute = true;
        break;
    default: return;
    }

    bool didSwap = false;
    if (canCommute && left && right && IsConstExpr(*left)) {
        AST *swap = *left;
        *left = *right;
        *right = swap;
        didSwap = true;
    }

    if (right && *right && IsConstExpr(*right)) {
        int32_t rightVal = EvalConstExpr(*right);

        if (canZeroOpt && rightVal == zeroOptVal) {
            AstReportAs(*right,&save);
            *optoken = '+';
            *left = AstInteger(0);
            *right = AstInteger(0);
            AstReportDone(&save);
            return;
        } else if (canNopOpt && rightVal == nopOptVal) {
            if (left && *left && (*left)->kind == AST_OPERATOR) {
                *optoken = (*left)->d.ival;
                *right = (*left)->right;
                *left = (*left)->left;
                OptimizeOperator(optoken,left,right);
                return;
            } else {
                AstReportAs(*right,&save);
                *optoken = '+';
                *right = AstInteger(0);
                AstReportDone(&save);
                return;
            }
        } else if (shiftOptOp && isPowerOf2(rightVal)) {
            AstReportAs(*right,&save);
            *optoken = shiftOptOp;
            *right = AstInteger(31-__builtin_clz(rightVal));
            AstReportDone(&save);
            return;
        }
    }
    if (didSwap) {
        // Undo swap if we couldn't do anything (fixes "waitcnt(381+cnt)"")
        AST *swap = *left;
        *left = *right;
        *right = swap;
    }
}

static bool IsConstZero(AST *ast) {
    return IsConstExpr(ast) && EvalConstExpr(ast) == 0;
}

static void StringAppend(Flexbuf *fb,AST *expr) {
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
 
static void BCCompileExpression(BCIRBuffer *irbuf,AST *node,BCContext context,bool asStatement); // forward decl;
static void BCCompileStatement(BCIRBuffer *irbuf,AST *node, BCContext context); // forward decl;

static ByteOpIR*
BCNewOrphanLabel(BCContext context) {
    ByteOpIR *lbl = calloc(sizeof(ByteOpIR),1);
    lbl->kind = BOK_LABEL;
    lbl->attr.labelHiddenVars = context.hiddenVariables;
    return lbl;
}

static ByteOpIR*
BCPushLabel(BCIRBuffer *irbuf,BCContext context) {
    ByteOpIR lbl = {.kind = BOK_LABEL,.attr.labelHiddenVars = context.hiddenVariables};
    return BIRB_PushCopy(irbuf,&lbl);
}

static void
BCCompileInteger(BCIRBuffer *irbuf,int32_t ival) {
    ByteOpIR opc = {0};
    opc.kind = BOK_CONSTANT;
    opc.data.int32 = ival;
    BIRB_PushCopy(irbuf,&opc);
}

static void BCCompilePopN(BCIRBuffer *irbuf,int popcount) {
    if (popcount < 0) ERROR(NULL,"Internal Error: negative pop count");
    else if (popcount > 0) {
        BCCompileInteger(irbuf,popcount*4);
        ByteOpIR popOp = {.kind = BOK_POP};
        BIRB_PushCopy(irbuf,&popOp);
    }
}

static void
BCCompileJumpEx(BCIRBuffer *irbuf, ByteOpIR *label, enum ByteOpKind kind, BCContext context, int stackoffset) {
    int stackdiff = context.hiddenVariables - label->attr.labelHiddenVars + stackoffset;
    if (stackdiff > 0) {
        // emit pop to get rid of hidden vars
        BCCompilePopN(irbuf,stackdiff);
    } else if (stackdiff < 0) {
        ERROR(NULL,"Internal Error: Compiling jump to label with more hidden vars than current context");
    }
    ByteOpIR condjmp = {0};
    condjmp.kind = kind;
    condjmp.jumpTo = label;
    BIRB_PushCopy(irbuf,&condjmp);
}

static void
BCCompileJump(BCIRBuffer *irbuf, ByteOpIR *label, BCContext context) {
    BCCompileJumpEx(irbuf,label,BOK_JUMP,context,0);
}

static void
BCCompileConditionalJump(BCIRBuffer *irbuf,AST *condition, bool ifNotZero, ByteOpIR *label, BCContext context) {
    ByteOpIR condjmp = {0};
    condjmp.jumpTo = label;
    int stackdiff = context.hiddenVariables - label->attr.labelHiddenVars;
    if (stackdiff) ERROR(condition,"Conditional jump to label with unequal hidden var count");
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
    } else if (condition->kind == AST_OPERATOR && (condition->d.ival == K_EQ || condition->d.ival == K_NE) 
    && ( IsConstZero(condition->left) || IsConstZero(condition->right))) {
        // Slightly complex condition, I know
        // optimize conditions like x == 0 and x<>0
        BCCompileConditionalJump(irbuf,IsConstZero(condition->left) ? condition->right : condition->left,!!ifNotZero != !!(condition->d.ival == K_EQ),label,context);
        return;
    } else if (condition->kind == AST_OPERATOR && condition->d.ival == K_BOOL_AND && !ifNotZero) {
        // like in "IF L AND R"
        BCCompileConditionalJump(irbuf,condition->left,false,label,context);
        BCCompileConditionalJump(irbuf,condition->right,false,label,context);
        return;
    } else if (condition->kind == AST_OPERATOR && condition->d.ival == K_BOOL_AND && ifNotZero) {
        // like in "IFNOT L AND R"
        ByteOpIR *skipLabel = BCNewOrphanLabel(context);
        BCCompileConditionalJump(irbuf,condition->left,false,skipLabel,context);
        BCCompileConditionalJump(irbuf,condition->right,true,label,context);
        BIRB_Push(irbuf,skipLabel);
        return;
    } else if (condition->kind == AST_OPERATOR && condition->d.ival == K_BOOL_OR && !ifNotZero) {
        // like in "IF L OR R"
        ByteOpIR *skipLabel = BCNewOrphanLabel(context);
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

enum MemOpTargetKind {
   MOT_UHHH,MOT_MEM,MOT_REG,MOT_REGBIT,MOT_REGBITRANGE
};

static void
BCCompileMemOpExEx(BCIRBuffer *irbuf,AST *node,BCContext context, enum MemOpKind kind, 
    enum MathOpKind modifyMathKind, bool modifyReverseMath, bool pushModifyResult, ByteOpIR *jumpTo, bool repeatPopStep) {
    
    enum MemOpTargetKind targetKind = 0;

    if (jumpTo != NULL) {
        if (kind != MEMOP_MODIFY || modifyMathKind != MOK_MOD_REPEATSTEP) ERROR(node,"Trying to compile memop with jumpTo that is not a jumping kind");
        int stackdiff = context.hiddenVariables - jumpTo->attr.labelHiddenVars;
        if (stackdiff) ERROR(node,"Modify jump to label with unequal hidden var count");
    }
    
    ByteOpIR memOp = {.mathKind = modifyMathKind,
        .attr.memop = {.modifyReverseMath = modifyReverseMath,.pushModifyResult = pushModifyResult,.repeatPopStep=repeatPopStep},
        .jumpTo=jumpTo};

    AST *type = NULL;
    AST *typeoverride = NULL;
    Symbol *sym = NULL;
    HwReg *hwreg;
    AST *baseExpr = NULL;
    AST *indexExpr = NULL;
    AST *bitExpr1 = NULL;
    AST *bitExpr2 = NULL;

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
            DEBUG(node,"handling size override...");
            typeoverride = ident->left;
            ident = ident->right->left->left;
            goto try_ident_again;
        } else {
            // normal raw memory access
            memOp.attr.memop.base = MEMOP_BASE_POP;
            targetKind = MOT_MEM;

            type = ident->left;
            baseExpr = ident->right;
            goto nosymbol_memref;
        }
    } else if (ident->kind == AST_HWREG) {
        targetKind = MOT_REG;
        hwreg = (HwReg *)ident->d.ptr;
        memOp.data.int32 = HWReg2Index(hwreg);
        memOp.attr.memop.memSize = MEMOP_SIZE_LONG;
        goto after_typeinfer;
    } else if (ident->kind == AST_RANGEREF) {
        ASSERT_AST_KIND(ident->left,AST_HWREG,return;);
        ASSERT_AST_KIND(ident->right,AST_RANGE,return;);
        hwreg = (HwReg *)ident->left->d.ptr;
        memOp.data.int32 = HWReg2Index(hwreg);
        memOp.attr.memop.memSize = MEMOP_SIZE_LONG;

        bitExpr1 = ident->right->left;
        bitExpr2 = ident->right->right;

        targetKind = bitExpr2 ? MOT_REGBITRANGE : MOT_REGBIT;
        goto after_typeinfer;
    }

    if (!sym) {
        ERROR(ident,"Can't get symbol");
        return;
    } else {
        //printf("got symbol with name %s and kind %d\n",sym->our_name,sym->kind);
        type = ExprType(ident);
        switch (sym->kind) {
        case SYM_LABEL: {
            memOp.attr.memop.base = MEMOP_BASE_PBASE;
            targetKind = MOT_MEM;

            Label *lab = sym->val;
            uint32_t labelval = lab->hubval;
            // Add header offset
            labelval += BCgetDAToffset(current,false,node,true);
            memOp.data.int32 = labelval;
        } break;
        case SYM_VARIABLE: {
            if (!strcmp(sym->our_name,"__clkfreq_var") || !strcmp(sym->our_name,"__clkmode_var")) {
                // FIXME figure out how to properly differentiate these
                DEBUG(node,"Got special symbol %s with offset %d",sym->our_name,sym->offset);
                memOp.attr.memop.base = MEMOP_BASE_POP;
                targetKind = MOT_MEM;
                if (baseExpr) ERROR(node,"baseExpr already set?!?!");
                else baseExpr = AstInteger(sym->offset);
            } else {
                memOp.attr.memop.base = MEMOP_BASE_VBASE;
                targetKind = MOT_MEM;
                memOp.data.int32 = sym->offset;
            }

        } break;
        case SYM_LOCALVAR: {
            memOp.attr.memop.base = MEMOP_BASE_DBASE;
            targetKind = MOT_MEM;
            memOp.data.int32 = sym->offset + BCLocalBase();
        } break;
        case SYM_PARAMETER: {
            memOp.attr.memop.base = MEMOP_BASE_DBASE;
            targetKind = MOT_MEM;
            memOp.data.int32 = sym->offset + BCParameterBase();
        } break;
        case SYM_TEMPVAR: {
            memOp.attr.memop.base = MEMOP_BASE_DBASE;
            targetKind = MOT_MEM;
            memOp.data.int32 = sym->offset + BCLocalBase();
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
            memOp.attr.memop.base = MEMOP_BASE_DBASE;
            targetKind = MOT_MEM;
            memOp.data.int32 = sym->offset + BCResultsBase();
        } break;   
        case SYM_HWREG: {
            targetKind = MOT_REG;
            hwreg = (HwReg *)sym->val;
            memOp.data.int32 = HWReg2Index(hwreg);
            memOp.attr.memop.memSize = MEMOP_SIZE_LONG;
            goto after_typeinfer;
        } break;
        case SYM_BUILTIN: {
            if (!strcmp(sym->our_name,"_cogid")) {
                if (kind != MEMOP_READ) WARNING(node,"Writing COGID register, bad idea");
                Symbol *cogid_sym = LookupSymbolInTable(&spinCommonReservedWords,"__interp_cogid");
                if (!cogid_sym || cogid_sym->kind != SYM_HWREG) {
                    ERROR(node,"Internal Error: Failed to get cogid register");
                    return;
                }
                targetKind = MOT_REG;
                hwreg = (HwReg *)cogid_sym->val;
                memOp.data.int32 = HWReg2Index(hwreg);
                memOp.attr.memop.memSize = MEMOP_SIZE_LONG;
                goto after_typeinfer;
            } else {
                ERROR(node,"Unhandled symbol identifier %s in memop",sym->our_name);
                return;
            }
        } break;
        default:
            ERROR(ident,"Unhandled Symbol type %d in memop",sym->kind);
            return;
        }
    }
    nosymbol_memref:
    
    if (typeoverride) type = typeoverride;
    if (IsPointerType(type))
        type = ast_type_long;
    else type = RemoveTypeModifiers(BaseType(type));
    
    // FIXME: should we actually use TYPESIZE here rather than relying on
    // particular types?
         if (type == ast_type_byte) memOp.attr.memop.memSize = MEMOP_SIZE_BYTE;
    else if (type == ast_type_word) memOp.attr.memop.memSize = MEMOP_SIZE_WORD; 
    else if (type == ast_type_long) memOp.attr.memop.memSize = MEMOP_SIZE_LONG;
    // \/ hack \/
    else if (type == ast_type_unsigned_long && (kind != MEMOP_MODIFY || modifyMathKind == MOK_MOD_WRITE)) memOp.attr.memop.memSize = MEMOP_SIZE_LONG;
    else if (type == NULL) memOp.attr.memop.memSize = MEMOP_SIZE_LONG; // Assume long type... This is apparently neccessary
    else {
        ERROR(node,"Can't figure out mem op type, is unhandled");
        printASTInfo(type);
    }

    after_typeinfer:

    memOp.attr.memop.modSize = memOp.attr.memop.memSize; // Let's just assume these are the same

    switch(targetKind) {
    case MOT_MEM: {
        if (!!baseExpr != !!(memOp.attr.memop.base == MEMOP_BASE_POP)) ERROR(node,"Internal Error: baseExpr condition mismatch");
        if (bitExpr1) ERROR(node,"Bit1 expression on memory op!");
        if (bitExpr2) ERROR(node,"Bit2 expression on memory op!");
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
        switch (kind) {
        case MEMOP_READ: memOp.kind = BOK_MEM_READ; break;
        case MEMOP_WRITE: memOp.kind = BOK_MEM_WRITE; break;
        case MEMOP_MODIFY: memOp.kind = BOK_MEM_MODIFY; break;
        case MEMOP_ADDRESS: memOp.kind = BOK_MEM_ADDRESS; break;
        default: ERROR(node,"Unknown memop kind %d",kind); break;
        }
    } break;
    case MOT_REG: {
        if (baseExpr) ERROR(node,"Base expression on plain register op!");
        if (indexExpr) ERROR(node,"Index expression on plain register op!");
        if (bitExpr1) ERROR(node,"Bit1 expression on plain register op!");
        if (bitExpr2) ERROR(node,"Bit2 expression on plain register op!");
        if (memOp.attr.memop.memSize != MEMOP_SIZE_LONG) ERROR(node,"Non-long size on plain register op!");
        switch (kind) {
        case MEMOP_READ: memOp.kind = BOK_REG_READ; break;
        case MEMOP_WRITE: memOp.kind = BOK_REG_WRITE; break;
        case MEMOP_MODIFY: memOp.kind = BOK_REG_MODIFY; break;
        case MEMOP_ADDRESS: ERROR(node,"Trying to get address of register"); break;
        default: ERROR(node,"Unknown memop kind %d",kind); break;
        }
    } break;
    case MOT_REGBIT: {
        if (baseExpr) ERROR(node,"Base expression on register bit op!");
        if (indexExpr) ERROR(node,"Index expression on register bit op!");
        if (bitExpr2) ERROR(node,"Bit2 expression on register bit op!");
        if (memOp.attr.memop.memSize != MEMOP_SIZE_LONG) ERROR(node,"Non-long size on register bit op!");

        memOp.attr.memop.modSize = MEMOP_SIZE_BIT; // I guess?

        BCCompileExpression(irbuf,bitExpr1,context,false);

        switch (kind) {
        case MEMOP_READ: memOp.kind = BOK_REGBIT_READ; break;
        case MEMOP_WRITE: memOp.kind = BOK_REGBIT_WRITE; break;
        case MEMOP_MODIFY: memOp.kind = BOK_REGBIT_MODIFY; break;
        case MEMOP_ADDRESS: ERROR(node,"Trying to get address of register"); break;
        default: ERROR(node,"Unknown memop kind %d",kind); break;
        }
    } break;
    case MOT_REGBITRANGE: {
        if (baseExpr) ERROR(node,"Base expression on register range op!");
        if (indexExpr) ERROR(node,"Index expression on register range op!");
        if (memOp.attr.memop.memSize != MEMOP_SIZE_LONG) ERROR(node,"Non-long size on register range op!");
        
        memOp.attr.memop.modSize = MEMOP_SIZE_BIT; // I guess?

        BCCompileExpression(irbuf,bitExpr1,context,false);
        BCCompileExpression(irbuf,bitExpr2,context,false);

        switch (kind) {
        case MEMOP_READ: memOp.kind = BOK_REGBITRANGE_READ; break;
        case MEMOP_WRITE: memOp.kind = BOK_REGBITRANGE_WRITE; break;
        case MEMOP_MODIFY: memOp.kind = BOK_REGBITRANGE_MODIFY; break;
        case MEMOP_ADDRESS: ERROR(node,"Trying to get address of register"); break;
        default: ERROR(node,"Unknown memop kind %d",kind); break;
        }
    } break;
    default: {
        ERROR(node,"Internal Error: memop targetKind not set/unhandled");
    } break;
    }

    BIRB_PushCopy(irbuf,&memOp);
}

static inline void
BCCompileMemOpEx(BCIRBuffer *irbuf,AST *node,BCContext context, enum MemOpKind kind, enum MathOpKind modifyMathKind, bool modifyReverseMath, bool pushModifyResult) {
    BCCompileMemOpExEx(irbuf,node,context,kind,modifyMathKind,modifyReverseMath,pushModifyResult,NULL,false);
}

static inline void
BCCompileMemOp(BCIRBuffer *irbuf,AST *node,BCContext context, enum MemOpKind kind) {
    BCCompileMemOpEx(irbuf,node,context,kind,0,false,false);
}

static void
BCCompileAssignment(BCIRBuffer *irbuf,AST *node,BCContext context,bool asExpression,enum MathOpKind modifyMathKind) {
    ASSERT_AST_KIND(node,AST_ASSIGN,;);
    AST *left = node->left, *right = node->right;
    bool modifyReverseMath = false;

    // This only happens with assignments where the lhs has side effects
    if (node->d.ival != K_ASSIGN) {
        if (modifyMathKind) ERROR(node,"direct operator AND given modfiyMathKind in AST_ASSIGN is unhandled");
        bool isUnary = false;
        enum MathOpKind mok = 0;
        int optoken = node->d.ival;
        OptimizeOperator(&optoken,NULL,&right);
        switch (optoken) {
        case K_SIGNEXTEND:
            if (right && IsConstExpr(right)) {
                int32_t extendlen = EvalConstExpr(right);
                if (extendlen == 8 || extendlen == 16) {
                    mok = (extendlen==16)?MOK_MOD_SIGNX_WORD:MOK_MOD_SIGNX_BYTE;
                    right = NULL;
                    isUnary = true;
                } else {
                    ERROR(node,"Sign-extend must be 8 or 16 bits");
                }
            } else {
                ERROR(node,"Sign-extend must have constant length");
            }
            break;
        default: 
            mok = Optoken2MathOpKind(optoken,&isUnary);
            break;
        }

        // Handle no-ops
        if (mok == MOK_ADD && IsConstExpr(right) && EvalConstExpr(right) == 0) {
            if (asExpression || ExprHasSideEffects(left)) BCCompileExpression(irbuf,left,context,!asExpression);
            return;
        }

        if (!mok) ERROR(node,"direct operator %03X in AST_ASSIGN is unhandled",optoken);
        modifyMathKind = mok;
    }

    // Try to contract things like "a := a + 1" into a modify op
    if (modifyMathKind == 0 && right && right->kind == AST_OPERATOR) {
        bool isUnary = false;
        enum MathOpKind mok = 0;
        int optoken = right->d.ival;
        AST *opleft = right->left;
        AST *opright = right->right;
        OptimizeOperator(&optoken,&opleft,&opright);
        switch (optoken) {
        case K_SIGNEXTEND:
            if (opright && IsConstExpr(opright)) {
                int32_t extendlen = EvalConstExpr(opright);
                if (extendlen == 8 || extendlen == 16) {
                    mok = (extendlen==16)?MOK_MOD_SIGNX_WORD:MOK_MOD_SIGNX_BYTE;
                    opright = opleft;
                    opleft = NULL;
                    isUnary = true;
                } else {
                    ERROR(node,"Sign-extend must be 8 or 16 bits");
                }
            } else {
                ERROR(node,"Sign-extend must have constant length");
            }
            break;
        default: mok = Optoken2MathOpKind(optoken,&isUnary); break;
        }
        if (mok == 0) goto nocontract; // Can't handle this operator

        AST *operand;
        
        if (isUnary && AstMatch(left,opright)) {
            operand = NULL;
            modifyReverseMath = false;
        } else if (!isUnary && AstMatch(left,opleft)) { // "a := a +1"
            operand = opright;
            modifyReverseMath = false;
            // Handle no-ops
            if (mok == MOK_ADD && IsConstExpr(opright) && EvalConstExpr(opright) == 0) {
                if (asExpression || ExprHasSideEffects(opleft)) BCCompileExpression(irbuf,opleft,context,!asExpression);
                return;
            }
        } else if (!isUnary && AstMatch(left,opright)) { // "a := 1 + a"
            operand = opleft;
            modifyReverseMath = true;
        } else goto nocontract;

        if (ExprHasSideEffects(left)) goto nocontract;

        // Do contraction
        modifyMathKind = mok;
        right = operand;
    }
    nocontract:

    if (modifyMathKind == 0 && asExpression) modifyMathKind = MOK_MOD_WRITE;

    bool isUnaryModify = isUnaryOperator(modifyMathKind);

    if (isUnaryModify && right) {
        ERROR(node,"Unary modify-assign with right side??? (math kind %s)",mathOpKindNames[modifyMathKind]);
        return;
    } else if (!isUnaryModify && !right) {
        ERROR(node,"Non-unary modify-assign without right side??? (math kind %s)",mathOpKindNames[modifyMathKind]);
        return;
    }

    if (isUnaryModify && modifyReverseMath) ERROR(node,"Reversed unary math??");

    AST *memopNode = NULL;

    switch(left->kind) {
    case AST_RANGEREF:
    case AST_HWREG: {
        memopNode = left;
    } break;
    case AST_ARRAYREF: {
        memopNode = left;
    } break;
    case AST_RESULT: {
        memopNode = left;
    } break;
    case AST_SYMBOL:
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER: {
        Symbol *sym = LookupAstSymbol(left,NULL);
        if (!sym) {
            ERROR(node,"Internal Error: no symbol in identifier assign");
            return;
        }
        switch (sym->kind) {
        case SYM_TEMPVAR: DEBUG(node,"temp variable %s used",sym->our_name); // fall through
        case SYM_VARIABLE:
        case SYM_LOCALVAR:
        case SYM_PARAMETER:
        case SYM_RESULT:
            memopNode = left;
            break;
        case SYM_RESERVED:
            if (!strcmp(sym->our_name,"result")) {
                // FIXME the parser should give AST_RESULT here, I think
                memopNode = NewAST(AST_RESULT,left->left,left->right);
            } else {
                ERROR(node,"Unhandled reserved word %s in assignment",sym->our_name);
            }
            break;
        default:
            ERROR(left,"Unhandled Identifier symbol kind %d in assignment",sym->kind);
            return;
        }
    } break;
    default:
        ERROR(left,"Unhandled assign left kind %d",left->kind);
        break;
    }

    if (!memopNode) {
        ERROR(node,"Internal error: no memopNode");
        return;
    }

    

    if (modifyMathKind == 0) {
        if (asExpression) ERROR(node,"Internal Error: shouldn't happen");
        BCCompileExpression(irbuf,right,context,false);
        BCCompileMemOp(irbuf,memopNode,context,MEMOP_WRITE);
    } else {
        if (!isUnaryModify) BCCompileExpression(irbuf,right,context,false);
        BCCompileMemOpEx(irbuf,memopNode,context,MEMOP_MODIFY,modifyMathKind,modifyReverseMath,asExpression);
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
        if (!IsIdentifier(ident)) {
            ERROR(ident, "Expected identifier");
            return 0;
        }
        if(strcmp(GetIdentifierName(ident),name)) continue; 
        if (gettype) *gettype = obj->left;
        return i+1+ModData(M)->pub_cnt+ModData(M)->pri_cnt;
    }
    ERROR(NULL,"can't find OBJ id for %s",name);
    return 0;
}

// FIXME this seems very convoluted
static int getObjIDByClass(Module *M, AST *classCast, AST** gettype) {
    Module *classptr = GetClassPtr(classCast->left);
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
        Module *ptr = GetClassPtr(obj->left);
        ASSERT_AST_KIND(ident,AST_IDENTIFIER,return 0;);
        if (ptr != classptr) continue; 
        if (gettype) *gettype = obj->left;
        return i+1+ModData(M)->pub_cnt+ModData(M)->pri_cnt;
    }
    ERROR(classCast,"can't find OBJ id for cast value");
    return 0;
}

static void
BCCompileFunCall(BCIRBuffer *irbuf,AST *node,BCContext context, bool asExpression, bool rescueAbort) {

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
        if (ident->kind == AST_CAST) {
            // assume this must be a call to a
            // static method and use any available object
            // (this may not actually always work, but it's enough to
            // get us going)
            callobjid = getObjIDByClass(current, ident, &objtype);
        } else {
            ASSERT_AST_KIND(ident,AST_IDENTIFIER,return;);
            Symbol *objsym = LookupAstSymbol(ident,NULL);
            if(!objsym || objsym->kind != SYM_VARIABLE) {
                ERROR(node->left,"Internal error: Invalid call receiver");
                return;
            }

            callobjid = getObjID(current,objsym->our_name,&objtype);
        }
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
        if (node->left && IsIdentifier(node->left)) {
            sym = LookupAstSymbol(node->left, NULL);
        } else if (IsIdentifier(node)) {
            sym = LookupAstSymbol(node, NULL);
        } else {
            ERROR(node, "Function call is not an identifier");
            return;
        }

        if (sym && sym->module) {
            Module *P = (Module *)sym->module;
            if (P == systemModule && current != P) {
                callobjid = getObjID(current, P->classname, &objtype);
                if (callobjid<0) {
                    ERROR(node->left, "Not an OBJ of this object");
                    return;
                }
            }
        }
    }

    ByteOpIR callOp = {0};
    ByteOpIR anchorOp = {0};
    anchorOp.kind = BOK_ANCHOR;
    anchorOp.attr.anchor.withResult = asExpression;
    anchorOp.attr.anchor.rescue = rescueAbort;
    if (!sym) {
        ERROR(node,"Function call has no symbol");
        return;
    } else if (sym->kind == SYM_BUILTIN) {
        anchorOp.kind = 0; // Where we're going, we don't need Stack Frames
        if (rescueAbort) ERROR(node,"Can't rescue on builtin call!");

        if (!strcmp(sym->our_name,"_locknew")) {
            callOp.kind = BOK_LOCKNEW;
            callOp.attr.coginit.pushResult = asExpression;
        } else if (!strcmp(sym->our_name,"_lockset")) {
            callOp.kind = BOK_LOCKSET;
            callOp.attr.coginit.pushResult = asExpression;
        } else if (!strcmp(sym->our_name,"_lockclr")) {
            callOp.kind = BOK_LOCKCLR;
            callOp.attr.coginit.pushResult = asExpression;
        } else if (asExpression) {
            if (!strcmp(sym->our_name,"__builtin_strlen")) {
                callOp.kind = BOK_BUILTIN_STRSIZE;
            } else if (!strcmp(sym->our_name,"strcomp")) {
                callOp.kind = BOK_BUILTIN_STRCOMP;
            } else {
                ERROR(node,"Unhandled expression builtin %s",sym->our_name);
                return;
            }
        } else {
            if (!strcmp(sym->our_name,"_lockret")) {
                callOp.kind = BOK_LOCKRET;
            } else if (!strcmp(sym->our_name,"waitcnt")) {
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
                    DEBUG(node,"Got reboot!");
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
        DEBUG(node,"Got Spin coginit with function %s",calledmethod->name);
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
                DEBUG(node,"Compiling coginit call argument...");
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
        DEBUG(node,"Got PASM coginit");

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
    initOp.attr.coginit.pushResult = asExpression;
    BIRB_PushCopy(irbuf,&initOp);
}

static void
BCCompileExpression(BCIRBuffer *irbuf,AST *node,BCContext context,bool asStatement) {
    if(!node) {
        ERROR(NULL,"Internal Error: Null expression!!");
        return;
    }
    if (IsConstExpr(node)) {
        if (!asStatement) {
            int32_t val = EvalConstExpr(node);
            BCCompileInteger(irbuf,val);
        }
    } else {
        unsigned popResults = asStatement ? 1 : 0;
        switch(node->kind) {
            case AST_FUNCCALL: {
                BCCompileFunCall(irbuf,node,context,!asStatement,false);
                popResults = 0;
            } break;
            case AST_CATCH: {
                BCCompileFunCall(irbuf,node->left,context,!asStatement,true);
                popResults = 0;
            } break;
            case AST_COGINIT: {
                BCCompileCoginit(irbuf,node,context,true);
            } break;
            case AST_ASSIGN: {
                BCCompileAssignment(irbuf,node,context,!asStatement,0);
                popResults = 0;
            } break;
            case AST_SEQUENCE: {
                DEBUG(node,"got AST_SEQUENCE, might have been pessimized");
                BCCompileExpression(irbuf,node->left,context,true);
                if (node->right) BCCompileExpression(irbuf,node->right,context,asStatement);
                popResults = 0;
            } break;
            case AST_CONDRESULT: {
                // Ternary operator
                if (curfunc->language == LANG_SPIN_SPIN1) DEBUG(node,"got AST_CONDRESULT in Spin1?");
                ASSERT_AST_KIND(node->right,AST_THENELSE,break;);
                ByteOpIR *elselbl = BCNewOrphanLabel(context), *endlbl = BCNewOrphanLabel(context);
                BCCompileConditionalJump(irbuf,node->left,false,elselbl,context);
                BCCompileExpression(irbuf,node->right->left,context,false);
                BCCompileJump(irbuf,endlbl,context);
                BIRB_Push(irbuf,elselbl);
                BCCompileExpression(irbuf,node->right->right,context,false);
                BIRB_Push(irbuf,endlbl);
            } break;
            case AST_OPERATOR: {
                enum MathOpKind mok;
                bool unary = false;
                AST *left = node->left,*right = node->right;
                int optoken = node->d.ival;
                OptimizeOperator(&optoken,&left,&right);
                switch(optoken) {

                case K_BOOL_AND:
                case K_BOOL_OR: {
                    bool is_and = optoken == K_BOOL_AND;
                    if (!ExprHasSideEffects(right)) {
                        // Use logic instead
                        mok = is_and ? MOK_LOGICAND : MOK_LOGICOR;
                        break;
                    } else {
                        ByteOpIR notOp = {.kind = BOK_MATHOP,.mathKind = MOK_BOOLNOT};
                        // build rickety short circuit op
                        ByteOpIR *lbl = BCNewOrphanLabel(context);
                        BCCompileInteger(irbuf,is_and ? 0 : -1);
                        BCCompileConditionalJump(irbuf,left,!is_and,lbl,context);
                        BCCompileConditionalJump(irbuf,right,!is_and,lbl,context);
                        BIRB_PushCopy(irbuf,&notOp);
                        BIRB_Push(irbuf,lbl);
                        return;
                    }
                } break;

                case '+': 
                    // OptimizeOpertor turns no-ops into val + 0, so handle that here
                    if (IsConstExpr(right) && EvalConstExpr(right) == 0) goto noOp;
                    mok = MOK_ADD;
                    break;

                case K_SIGNEXTEND:
                case K_ZEROEXTEND:
                    {
                        if (ExprHasSideEffects(right)) {
                            ERROR(node, "Bytecode output cannot handle side effects in right argument of sign/zero extend");
                            right = AstInteger(16);
                        }
                        BCCompileExpression(irbuf, left, context, false);
                        BCCompileExpression(irbuf, right, context, false);
                        ByteOpIR mathOp = {0};
                        mathOp.kind = BOK_MATHOP;
                        mathOp.mathKind = MOK_SHL;
                        BIRB_PushCopy(irbuf, &mathOp);
                        mok = (optoken == K_SIGNEXTEND) ? MOK_SAR : MOK_SHR;
                        unary = true;
                    } break;
                case K_INCREMENT: 
                case K_DECREMENT:
                case '?':
                    goto modifyOp;
                default:
                    mok = Optoken2MathOpKind(optoken,&unary);    
                    if (!mok) {
                        ERROR(node,"Unhandled operator 0x%03X",optoken);
                        return;
                    }
                }

                if (!unary) BCCompileExpression(irbuf,left,context,false);
                BCCompileExpression(irbuf,right,context,false);

                ByteOpIR mathOp = {0};
                mathOp.kind = BOK_MATHOP;
                mathOp.mathKind = mok;
                BIRB_PushCopy(irbuf,&mathOp);

                break;

                noOp: {
                    if (asStatement) popResults = 0;
                    BCCompileExpression(irbuf,left,context,asStatement);
                } break;

                modifyOp: {// Handle operators that modfiy a variable

                    bool isPostfix = node->left;
                    AST *modvar = isPostfix?node->left:node->right;
                    if (asStatement) popResults = 0;

                    mok = 0;
                    switch(optoken) {
                    case K_INCREMENT: mok = isPostfix ? MOK_MOD_POSTINC : MOK_MOD_PREINC; break;
                    case K_DECREMENT: mok = isPostfix ? MOK_MOD_POSTDEC : MOK_MOD_PREDEC; break;
                    case '?': mok = isPostfix ? MOK_MOD_RANDBACKWARD : MOK_MOD_RANDFORWARD; break;
                    }
                    if (!mok) ERROR(node,"Unhandled %sfix modify operator %03X",isPostfix?"post":"pre",optoken);
                    
                    BCCompileMemOpEx(irbuf,modvar,context,MEMOP_MODIFY,mok,false,!asStatement);
                } break;
            } break;
            case AST_POSTSET: {
                switch(gl_interp_kind) {
                case INTERP_KIND_P1ROM: {
                    bool isConst = IsConstExpr(node->right);
                    int32_t constVal = isConst ? EvalConstExpr(node->right) : 0;
                    if (isConst && constVal == 0) BCCompileMemOpEx(irbuf,node->left,context,MEMOP_MODIFY,MOK_MOD_POSTCLEAR,false,!asStatement);
                    else if (isConst && constVal == -1) BCCompileMemOpEx(irbuf,node->left,context,MEMOP_MODIFY,MOK_MOD_POSTSET,false,!asStatement);
                    else {
                        // Read old value, eval new value, write back. Leaves old value on stack.
                        if (!asStatement) BCCompileMemOp(irbuf,node->left,context,MEMOP_READ);
                        BCCompileExpression(irbuf,node->right,context,false);
                        BCCompileMemOp(irbuf,node->left,context,MEMOP_WRITE);
                    }
                    popResults = 0;
                } break;
                default:
                    ERROR(NULL,"Unknown interpreter kind");
                    break;
                }
            } break;
            case AST_ISBETWEEN: {
                // TODO make this not terrible. Could really use a temp var here.
                DEBUG(node,"Compiling AST_ISBETWEEN");
                if (ExprHasSideEffects(node->left)) WARNING(node->left,"Compiling AST_ISBETWEEN with side-effect-having expression");
                BCCompileExpression(irbuf,node->left,context,false);
                BCCompileExpression(irbuf,node->right->left,context,false);
                ByteOpIR ae_op = {.kind = BOK_MATHOP,.mathKind = MOK_CMP_AE};
                BIRB_PushCopy(irbuf,&ae_op);
                BCCompileExpression(irbuf,node->left,context,false);
                BCCompileExpression(irbuf,node->right->right,context,false);
                ByteOpIR be_op = {.kind = BOK_MATHOP,.mathKind = MOK_CMP_BE};
                BIRB_PushCopy(irbuf,&be_op);
                ByteOpIR and_op = {.kind = BOK_MATHOP,.mathKind = MOK_LOGICAND};
                BIRB_PushCopy(irbuf,&and_op);
            } break;
            case AST_RANGEREF:
            case AST_HWREG: {
                BCCompileMemOp(irbuf,node,context,MEMOP_READ);
            } break;
            case AST_ABSADDROF: // Same thing in Spin code, I guess.
            case AST_ADDROF: {
                // Right always empty?
                if (node->right) WARNING(node->right,"right side of AST_ADDROF not empty???");
                BCCompileMemOp(irbuf,node->left,context,MEMOP_ADDRESS);
            } break;
            case AST_LOCAL_IDENTIFIER:
            case AST_IDENTIFIER: {
                Symbol *sym = LookupAstSymbol(node,NULL);
                if (!sym) {
                    ERROR(node,"Error: symbol %s not found", GetUserIdentifierName(node));
                    return;
                }
                switch (sym->kind) {
                case SYM_TEMPVAR: DEBUG(node,"temp variable %s used",sym->our_name); // fall through
                case SYM_VARIABLE:
                case SYM_LOCALVAR:
                case SYM_PARAMETER:
                case SYM_RESULT:
                case SYM_HWREG:
                    BCCompileMemOp(irbuf,node,context,MEMOP_READ);
                    break;
                case SYM_RESERVED:
                    if (!strcmp(sym->our_name,"result")) {
                        // FIXME the parser should give AST_RESULT here, I think
                        BCCompileMemOp(irbuf,NewAST(AST_RESULT,node->left,node->right),context,MEMOP_READ);
                    } else {
                        ERROR(node,"Unhandled reserved word %s in expression",sym->our_name);
                    }
                    break;
                case SYM_BUILTIN:
                    if (!strcmp(sym->our_name,"_cogid")) {
                        BCCompileMemOp(irbuf,node,context,MEMOP_READ);
                    } else {
                        // Try function call
                        BCCompileFunCall(irbuf,node,context,!asStatement,false);
                        popResults = 0;
                    }
                    break;
                default:
                    ERROR(node,"Unhandled Identifier symbol kind %d in expression",sym->kind);
                    return;
                }

            } break;
            case AST_RESULT: {
                BCCompileMemOp(irbuf,node,context,MEMOP_READ);
            } break;
            case AST_ARRAYREF: {
                BCCompileMemOp(irbuf,node,context,MEMOP_READ);
            } break;
            case AST_STRINGPTR: {
                ByteOpIR *stringLabel = BCNewOrphanLabel(nullcontext);
                ByteOpIR pushOp = {.kind = BOK_FUNDATA_PUSHADDRESS,.jumpTo = stringLabel,.attr.pushaddress.addPbase = true};
                ByteOpIR stringData = BCBuildString(node->left);

                BIRB_PushCopy(irbuf,&pushOp);
                BIRB_Push(irbuf->pending,stringLabel);
                BIRB_PushCopy(irbuf->pending,&stringData);

            } break;
            case AST_LOOKUP:
            case AST_LOOKDOWN: {
                bool isLookdown = node->kind == AST_LOOKDOWN;
                ASSERT_AST_KIND(node->left,AST_LOOKEXPR,return;);
                if (node->right->kind == AST_EXPRLIST) {
                    // Native lookup/down

                    // Compile base
                    BCCompileExpression(irbuf,node->left->left,context,false);
                    // Compile "done" jump pointer
                    ByteOpIR *afterLabel = BCNewOrphanLabel(context);
                    ByteOpIR pushOp = {.kind = BOK_FUNDATA_PUSHADDRESS,.jumpTo = afterLabel};
                    BIRB_PushCopy(irbuf,&pushOp);
                    // Compile the expression
                    BCCompileExpression(irbuf,node->left->right,context,false);

                    // Compile the items
                    for (AST *list=node->right;list;list=list->right) {
                        ASSERT_AST_KIND(list,AST_EXPRLIST,return;);
                        ByteOpIR lookOp = {0};
                        if (list->left->kind == AST_RANGE) {
                            BCCompileExpression(irbuf,list->left->left,context,false);
                            BCCompileExpression(irbuf,list->left->right,context,false);
                            lookOp.kind = isLookdown ? BOK_LOOKDOWN_RANGE : BOK_LOOKUP_RANGE;
                        } else {
                            BCCompileExpression(irbuf,list->left,context,false);
                            lookOp.kind = isLookdown ? BOK_LOOKDOWN : BOK_LOOKUP;
                        }
                        BIRB_PushCopy(irbuf,&lookOp);
                    }

                    ByteOpIR lookEnd = {.kind = BOK_LOOKEND};
                    BIRB_PushCopy(irbuf,&lookEnd);

                    BIRB_Push(irbuf,afterLabel);

                } else {
                    ERROR(node,"Unhandled LOOK* right AST kind %d",node->right->kind);
                }
            } break;
            case AST_DATADDROF: {
                ByteOpIR addPbaseOp = {.kind = BOK_MEM_ADDRESS,.attr.memop = {.base = MEMOP_BASE_PBASE,.memSize = MEMOP_SIZE_BYTE},.data.int32 = 0};
                if (IsConstExpr(node->left)) {
                    int32_t constVal = EvalConstExpr(node->left) & 0xFFFF; // TODO: P2 uses different / no mask?
                    if (constVal > 0x7FFF) goto expr_anyways; // Will cause compile failure for @@-1 and similar expressions otherwise
                    addPbaseOp.data.int32 = constVal; 
                } else {
                    expr_anyways:
                    BCCompileExpression(irbuf,node->left,context,false);
                    addPbaseOp.attr.memop.popIndex = true;
                }
                BIRB_PushCopy(irbuf,&addPbaseOp);
            } break;
            default:
                ERROR(node,"Unhandled node kind %d in expression",node->kind);
                return;
        }
        BCCompilePopN(irbuf,popResults);
    }
}

static void 
BCCompileStmtlist(BCIRBuffer *irbuf,AST *list, BCContext context) {

    for (AST *ast=list;ast&&ast->kind==AST_STMTLIST;ast=ast->right) {
        AST *node = ast->left;
        if (!node) {
            //ERROR(ast,"empty node?!?"); ignore, this can happen
            continue;
        }
        BCCompileStatement(irbuf,node,context);
    }

}

static void
BCCompileStatement(BCIRBuffer *irbuf,AST *node, BCContext context) {
    while (node && node->kind == AST_COMMENTEDNODE) {
        //printf("Node is allegedly commented:\n");
        // FIXME: this doesn't actually get any comments?
        /*
        for(AST *comment = node->right;comment&&comment->kind==AST_COMMENT;comment=comment->right) {
            printf("---  %s\n",comment->d.string);
        }*/
        node = node->left;
    }
    if (!node) return;
    
    switch(node->kind) {
    case AST_COMMENT:
        // for now, do nothing
        break;
    case AST_ASSIGN:
        BCCompileAssignment(irbuf,node,context,false,0);
        break;
    case AST_WHILE: {

        ByteOpIR *toplbl = BCPushLabel(irbuf,context);
        ByteOpIR *bottomlbl = BCNewOrphanLabel(context);

        // Compile condition
        BCCompileConditionalJump(irbuf,node->left,false,bottomlbl,context);

        ASSERT_AST_KIND(node->right,AST_STMTLIST,break;)

        BCContext newcontext = context;
        newcontext.nextLabel = toplbl;
        newcontext.quitLabel = bottomlbl;
        BCCompileStmtlist(irbuf,node->right,newcontext);

        BCCompileJump(irbuf,toplbl,context);
        BIRB_Push(irbuf,bottomlbl);
    } break;
    case AST_DOWHILE: {

        ByteOpIR *toplbl = BCPushLabel(irbuf,context);
        ByteOpIR *nextlbl = BCNewOrphanLabel(context);
        ByteOpIR *bottomlbl = BCNewOrphanLabel(context);

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
        if (IsSpinLang(curfunc->language)) DEBUG(node,"Got a FOR loop in Spin, probably wrong transform conditions");

        bool atleastonce = node->kind == AST_FORATLEASTONCE;

        ByteOpIR *topLabel = BCNewOrphanLabel(context);
        ByteOpIR *nextLabel = BCNewOrphanLabel(context);
        ByteOpIR *quitLabel = BCNewOrphanLabel(context);

        AST *initStmnt = node->left;
        AST *to = node->right;
        ASSERT_AST_KIND(to,AST_TO,return;);
        //printf("to: ");printASTInfo(to);
        AST *condExpression = to->left;
        AST *step = to->right;
        ASSERT_AST_KIND(step,AST_STEP,return;);
        //printf("step: ");printASTInfo(step);
        AST *nextExpression = step->left;
        AST *body = step->right;

        BCCompileStatement(irbuf,initStmnt,context);
        BIRB_Push(irbuf,topLabel);
        if(!atleastonce && condExpression) {
            BCCompileConditionalJump(irbuf,condExpression,false,quitLabel,context);
        }
        BCContext newcontext = context;
        newcontext.quitLabel = quitLabel;
        newcontext.nextLabel = nextLabel;
        if (body) {
            ASSERT_AST_KIND(body,AST_STMTLIST,return;);
            BCCompileStmtlist(irbuf,body,newcontext);
        } else DEBUG(node,"Compiling empty FOR loop?");

        BIRB_Push(irbuf,nextLabel);
        if (nextExpression) {
            BCCompileExpression(irbuf,nextExpression,context,true); // Compile as statement!
        }
        if(atleastonce) BCCompileConditionalJump(irbuf,condExpression,true,topLabel,context);
        else BCCompileJump(irbuf,topLabel,context);

        BIRB_Push(irbuf,quitLabel);

    } break;
    case AST_COUNTREPEAT: {

        AST *loopvar = node->left;
        
        AST *from = node->right;
        ASSERT_AST_KIND(from,AST_FROM,return;);
        //printf("from: ");printASTInfo(from);
        AST *fromExpression = from->left;
        AST *to = from->right;
        ASSERT_AST_KIND(to,AST_TO,return;);
        //printf("to: ");printASTInfo(to);
        AST *toExpression = to->left;
        AST *step = to->right;
        ASSERT_AST_KIND(step,AST_STEP,return;);
        //printf("step: ");printASTInfo(step);
        AST *stepExpression = step->left;
        AST *body = step->right;

        if (fromExpression == NULL && IsConstExpr(stepExpression) && abs(EvalConstExpr(stepExpression))==1) {
            // REPEAT N loop
            bool isConst = IsConstExpr(toExpression);
            uint32_t constVal = isConst ? EvalConstExpr(toExpression) : 0;
            if (isConst && constVal == 0) {
                DEBUG(node,"Skipping loop with zero repeats");
                break;
            }
            // Compile repeat count
            BCCompileExpression(irbuf,toExpression,context,false);

            BCContext newcontext = context;
            newcontext.hiddenVariables += 1;
            ByteOpIR *topLabel = BCNewOrphanLabel(newcontext);
            ByteOpIR *nextLabel = BCNewOrphanLabel(newcontext);
            ByteOpIR *quitLabel = BCNewOrphanLabel(context);
            newcontext.nextLabel = nextLabel;
            newcontext.quitLabel = quitLabel;

            if (!isConst || constVal == 0) BCCompileJumpEx(irbuf,quitLabel,BOK_JUMP_TJZ,newcontext,-1); 
            BIRB_Push(irbuf,topLabel);

            if (body) {
                ASSERT_AST_KIND(body,AST_STMTLIST,return;);
                BCCompileStmtlist(irbuf,body,newcontext);
            } else DEBUG(node,"Compiling empty REPEAT N loop?");

            BIRB_Push(irbuf,nextLabel);
            BCCompileJumpEx(irbuf,topLabel,BOK_JUMP_DJNZ,newcontext,0);
            BIRB_Push(irbuf,quitLabel);
        } else {
            // REPEAT FROM TO
            bool stepConst = IsConstExpr(stepExpression);
            uint32_t stepConstVal = stepConst ? EvalConstExpr(stepExpression) : 0;
            bool haveStep = !stepConst || abs(stepConstVal) != 1;

            // Compile init value
            BCCompileExpression(irbuf,fromExpression,context,false);
            BCCompileMemOp(irbuf,loopvar,context,MEMOP_WRITE);

            BCContext newcontext = context;
            ByteOpIR *topLabel = BCNewOrphanLabel(newcontext);
            ByteOpIR *nextLabel = BCNewOrphanLabel(newcontext);
            ByteOpIR *quitLabel = BCNewOrphanLabel(context);
            newcontext.nextLabel = nextLabel;
            newcontext.quitLabel = quitLabel;

            BIRB_Push(irbuf,topLabel);

            if (body) {
                ASSERT_AST_KIND(body,AST_STMTLIST,return;);
                BCCompileStmtlist(irbuf,body,newcontext);
            } else DEBUG(node,"Compiling empty REPEAT FROM TO loop?");

            BIRB_Push(irbuf,nextLabel);
            if (haveStep) BCCompileExpression(irbuf,stepExpression,newcontext,false);
            BCCompileExpression(irbuf,fromExpression,newcontext,false);
            BCCompileExpression(irbuf,toExpression,newcontext,false);
            BCCompileMemOpExEx(irbuf,loopvar,newcontext,MEMOP_MODIFY,MOK_MOD_REPEATSTEP,false,false,topLabel,haveStep);
            BIRB_Push(irbuf,quitLabel);
            
        }



    } break;
    case AST_IF: {

        ByteOpIR *bottomlbl = BCNewOrphanLabel(context);
        ByteOpIR *elselbl = BCNewOrphanLabel(context);

        AST *thenelse = node->right;
        if (thenelse && thenelse->kind == AST_COMMENTEDNODE) thenelse = thenelse->left;
        ASSERT_AST_KIND(thenelse,AST_THENELSE,break;);

        // Compile condition
        BCCompileConditionalJump(irbuf,node->left,false,elselbl,context);

        if(thenelse->left) { // Then
            ASSERT_AST_KIND(thenelse->left,AST_STMTLIST,break;);
            BCContext newcontext = context;
            BCCompileStmtlist(irbuf,thenelse->left,newcontext);
        }
        if(thenelse->right) BCCompileJump(irbuf,bottomlbl,context);
        BIRB_Push(irbuf,elselbl);
        if(thenelse->right) { // Else
            ASSERT_AST_KIND(thenelse->right,AST_STMTLIST,break;);
            BCContext newcontext = context;
            BCCompileStmtlist(irbuf,thenelse->right,newcontext);
        }
        if(thenelse->right) BIRB_Push(irbuf,bottomlbl);
    } break;
    case AST_CASE: {

        ByteOpIR *endlabel = BCNewOrphanLabel(context);
        ByteOpIR pushEnd = {.kind=BOK_FUNDATA_PUSHADDRESS,.jumpTo=endlabel};
        BIRB_PushCopy(irbuf,&pushEnd);
        // Compile switch expression
        BCCompileExpression(irbuf,node->left,context,false);

        ASSERT_AST_KIND(node->right,AST_STMTLIST,return;)

        BCContext newcontext = context;
        newcontext.hiddenVariables += 2;

        // Preview what we got
        int cases = 0;
        AST *othercase = NULL;
        //printf("Previewing cases\n");
        for(AST *list=node->right;list;list=list->right) {
            AST *item = list->left;
            if (item->kind == AST_COMMENTEDNODE) item = item->left;
            if (item->kind == AST_ENDCASE) {
                // Do nothing
                //printf("Got AST_ENDCASE\n");
            } else if (item->kind == AST_CASEITEM) {
                //printf("Got AST_CASEITEM\n");
                cases++;
            } else if (item->kind == AST_OTHER) {
                //printf("Got AST_OTHER\n");
                if (othercase) ERROR(item,"Multiple OTHER cases");
                othercase = item;
            } else {
                // Do nothing
            }
        }
        //printf("Got %d cases%s\n",cases,othercase?" and OTHER":"");
        ByteOpIR *caselabels[cases];
        for(int i=0;i<cases;i++) caselabels[i] = BCNewOrphanLabel(newcontext);
        ByteOpIR *otherlabel = othercase ? BCNewOrphanLabel(newcontext) : NULL; 
        
        // Generate case expressions
        //printf("Generating case expressions\n");
        int whichcase = 0;
        for(AST *list=node->right;list;list=list->right) {
            AST *item = list->left;
            if (item->kind == AST_COMMENTEDNODE) item = item->left;
            if (item->kind == AST_ENDCASE) {
                // Do nothing
                //printf("Got AST_ENDCASE\n");
            } else if (item->kind == AST_CASEITEM) {
                //printf("Got AST_CASEITEM\n");
                ASSERT_AST_KIND(item->left,AST_EXPRLIST,;);
                for (AST *exprlist=item->left;exprlist;exprlist=exprlist->right) {
                    ASSERT_AST_KIND(exprlist,AST_EXPRLIST,continue;);
                    ByteOpIR caseOp = {.jumpTo=caselabels[whichcase]};
                    if (exprlist->left->kind == AST_RANGE) {
                        //printf("... with range!\n");
                        BCCompileExpression(irbuf,exprlist->left->left,newcontext,false);
                        BCCompileExpression(irbuf,exprlist->left->right,newcontext,false);
                        caseOp.kind=BOK_CASE_RANGE;
                    } else {
                        //printf("... with expression?\n");
                        BCCompileExpression(irbuf,exprlist->left,newcontext,false);
                        caseOp.kind=BOK_CASE;
                    }
                    BIRB_PushCopy(irbuf,&caseOp);
                }
                whichcase++;
            } else if (item->kind == AST_OTHER) {
                // Do nothing
                //printf("Got AST_OTHER\n");
            } else {
                // Do nothing
            }
        }

        ByteOpIR noMatchDone = {.kind = BOK_CASE_DONE};
        if (othercase) {
            BCCompileJump(irbuf,otherlabel,newcontext);
        } else {
            BIRB_PushCopy(irbuf,&noMatchDone);
        }

        // Compile each case
        //printf("Compiling cases..\n");
        whichcase = 0;
        for(AST *list=node->right;list;list=list->right) {
            AST *item = list->left;
            if (item->kind == AST_COMMENTEDNODE) item = item->left;
            if (item->kind == AST_ENDCASE) {
                //printf("Got AST_ENDCASE\n");
                ByteOpIR endOp = {.kind = BOK_CASE_DONE};
                BIRB_PushCopy(irbuf,&endOp);
            } else if (item->kind == AST_CASEITEM || item->kind == AST_OTHER) {
                bool isOther = item->kind == AST_OTHER;
                //printf(isOther ? "Got AST_OTHER\n" : "Got AST_CASEITEM\n");
                BIRB_Push(irbuf,isOther?otherlabel:caselabels[whichcase]);
                AST *stmt = isOther ? item->left : item->right;
                if (stmt) {
                    if (stmt->kind == AST_COMMENTEDNODE) stmt = stmt->left;
                    if (stmt->kind == AST_ENDCASE) {
                        //printf("Got AST_ENDCASE inside AST_CASEITEM\n");
                        ByteOpIR endOp = {.kind = BOK_CASE_DONE};
                        BIRB_PushCopy(irbuf,&endOp);
                    } else {
                        BCCompileStatement(irbuf,stmt,newcontext);
                    }
                } else DEBUG(item,"Empty CASEITEM?");
                if (!isOther) whichcase++;
            } else {
                BCCompileStatement(irbuf,item,newcontext);
            }
        }


        BIRB_Push(irbuf,endlabel);

    } break;
    case AST_FUNCCALL: {
        BCCompileFunCall(irbuf,node,context,false,false);
    } break;
    case AST_CATCH: {
        BCCompileFunCall(irbuf,node->left,context,false,true);
    } break;
    case AST_CONTINUE: {
        if (!context.nextLabel) {
            ERROR(node,"NEXT outside loop");
        } else {
            BCCompileJump(irbuf,context.nextLabel,context);
        }
    } break;
    case AST_QUITLOOP: {
        if (!context.nextLabel) {
            ERROR(node,"QUIT outside loop");
        } else {
            BCCompileJump(irbuf,context.quitLabel,context);
        }
    } break;
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER: {
        Symbol *sym = LookupAstSymbol(node,NULL);
        if (!sym) ERROR(node,"Internal Error: Can't get symbol");
        switch (sym->kind) {
        case SYM_BUILTIN:
        case SYM_FUNCTION: {
            // This nonsense fixes REBOOT
            DEBUG(node,"identifier converted to function call for sym with kind %d and name %s",sym->kind,sym->our_name);
            AST *fakecall = NewAST(AST_FUNCCALL,node,NULL);
            BCCompileFunCall(irbuf,fakecall,context,false,false);
        } break;
        default: {
            ERROR(node,"Unhandled identifier symbol kind %d in statement",sym->kind);
        } break;
        }
    } break;
    case AST_COGINIT: {
        BCCompileCoginit(irbuf,node,context,false);
    } break;
    case AST_SEQUENCE: {
        BCCompileExpression(irbuf,node,context,true);
    } break;
    case AST_OPERATOR: {
        BCCompileExpression(irbuf,node,context,true);
    } break;
    case AST_THROW:
    case AST_RETURN: {
        // FIXME: Spin2 multi-return
        // TODO: This generates explicit returns of the result var. Fix in peephole step?
        bool isAbort = node->kind == AST_THROW;
        AST *retval = node->left;
        ByteOpIR returnOp = {0};
        returnOp.kind = isAbort ? (retval ? BOK_ABORT_POP : BOK_ABORT_PLAIN) : (retval ? BOK_RETURN_POP : BOK_RETURN_PLAIN);
        if (retval) BCCompileExpression(irbuf,retval,context,false);
        BIRB_PushCopy(irbuf,&returnOp);
    } break;
    case AST_YIELD: {
        // do nothing  
    } break;
    case AST_STMTLIST: {
        BCCompileStmtlist(irbuf,node,context);
    } break;
    default:
        ERROR(node,"Unhandled node kind %d in statement",node->kind);
        break;
    }
}


static void 
BCCompileFunction(ByteOutputBuffer *bob,Function *F) {

    DEBUG(NULL,"Compiling bytecode for function %s",F->name);
    curfunc = F; // Set this global, I guess;

    // first up, we now know the function's address, so let's fix it up in the header
    int func_ptr = bob->total_size;
    int func_offset = 0;
    int func_localsize = F->numlocals*4; // SUPER DUPER FIXME smaller locals
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
        DEBUG(NULL,"compiling function %s with no body...",F->name);
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

    BCIR_Optimize(&irbuf);

    BCIR_to_BOB(&irbuf,bob,func_ptr-ModData(current)->compiledAddress);
    
    curfunc = NULL;
}

static void
BCInsertModule(Module *P, Module *sub, const char *subname) {
    AST *classtype;
    AST *ident;
    
    ident = AstIdentifier(subname);
    classtype = ClassType(sub);

    DeclareOneMemberVar(P, ident, classtype, 1 /* is_private */);
    DeclareMemberVariables(P);
}

static void
BCPrepareObject(Module *P) {
    // Init bedata
    if (P->bedata) return;

    Module *save = current;
    current = P;

    DEBUG(NULL,"Preparing object %s",P->fullname);

    P->bedata = calloc(sizeof(BCModData), 1);
    ModData(P)->compiledAddress = -1;


    // Count and gather private/public methods
    {
        int pub_cnt = 0, pri_cnt = 0;
        for (Function *f = P->functions; f; f = f->next) {
            if (ShouldSkipFunction(f)) continue;

            if (f->is_public) ModData(P)->pubs[pub_cnt++] = f;
            else              ModData(P)->pris[pri_cnt++] = f;

            //printf("Got function %s\n",f->user_name);

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
                //printf("Got obj of type %s named %s\n",((Module*)var->left->d.ptr)->classname,var->right->left->d.string);
                arrsize = 1;
            } else if (var->right->left->kind == AST_ARRAYDECL) {
                ASSERT_AST_KIND(var->right->left->right,AST_INTEGER,return;);
                DEBUG(NULL,"Got obj array of type %s, size %d named %s",((Module*)var->left->d.ptr)->classname,var->right->left->right->d.ival,var->right->left->left->d.string);
                arrsize = var->right->left->right->d.ival;
            } else {
                ERROR(var->right,"Unhandled OBJ AST kind %d",var->right->left->kind);
            }
            //printASTInfo(var->right->left);
            //ASSERT_AST_KIND(var->right->left,AST_IDENTIFIER,return;);

            Module *objModule = GetClassPtr(var->left);
            BCPrepareObject(objModule);

            // Ignore empty objects
            if (ModData(objModule)->pub_cnt == 0 && ModData(objModule)->pri_cnt == 0 && ModData(objModule)->obj_cnt == 0 && objModule->varsize == 0) continue;

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

    current = save;
    
}

static void
BCCompileObject(ByteOutputBuffer *bob, Module *P) {

    Module *save = current;
    current = P;

    DEBUG(NULL,"Compiling bytecode for object %s",P->fullname);

    BOB_Align(bob,4); // Long-align

    BOB_Comment(bob,auto_printf(128,"--- Object Header for %s",P->classname));

    // insert system module
    if (systemModule && systemModule != P && systemModule->functions) {
        BCInsertModule(P, systemModule, "_system_");
    }
    // prepare object
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

    DEBUG(NULL,"Debug: this object (%s) has %d PUBs, %d PRIs and %d OBJs",P->classname,pub_cnt,pri_cnt,obj_cnt);

    OutputSpan *objOffsetSpans[obj_cnt];
    OutputSpan *sizeSpan;
    // Emit object header
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        sizeSpan = BOB_PushWord(bob,0,"Object size");
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

    // fixup header
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        BOB_Align(bob,4); // Do we need this?
        BOB_ReplaceWord(sizeSpan,bob->total_size - ModData(P)->compiledAddress,NULL);
        break;
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return;
    }

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
    current = save;
}

void OutputByteCode(const char *fname, Module *P) {
    FILE *bin_file = NULL,*lst_file = NULL;
    ByteOutputBuffer bob = {0};

    Module *save = current;
    current = P;

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
    const int headerSize = bob.total_size;

    BCCompileObject(&bob,P);

    BOB_Align(&bob,4);
    const int programSize = bob.total_size - headerSize;
    const int variableSize = P->varsize; // Already rounded up!
    const int stackBase = headerSize + programSize + variableSize + 8; // space for that stack init thing

    if (gl_printprogress) {
        printf("Program size:  %6d bytes\n",programSize);
        printf("Variable size: %6d bytes\n",variableSize);
        if (!gl_p2) printf("Stack/Free:    %6d bytes\n",0x8000-(headerSize+programSize+variableSize));
        else printf("Stack/Free:    %6d TODO\n",0);
    }

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

    // Fixup header
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        // TODO fixup everything
        BOB_ReplaceWord(headerspans.pcurr,FunData(mainFunc)->compiledAddress,NULL);
        BOB_ReplaceWord(headerspans.vbase,headerSize+programSize,NULL);
        BOB_ReplaceWord(headerspans.dbase,stackBase,NULL);
        BOB_ReplaceWord(headerspans.dcurr,stackBase+FunData(mainFunc)->localSize+mainFunc->numparams*4+4,NULL);
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
