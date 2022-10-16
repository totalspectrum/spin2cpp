//
// Bytecode compiler for spin2cpp
//
// Copyright 2021-2022 Ada Gottensträter and Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//

#include "outbc.h"
#include "bcbuffers.h"
#include "bcir.h"
#include <stdlib.h>
#include "becommon.h"
#include <inttypes.h>

const BCContext nullcontext = {.hiddenVariables = 0};

static int getFuncIDForKnownFunc(Module *M,Function *F);  // forward declaration
static void BCAddStaticReference(Module *P, AST *obj);

bool interp_can_multireturn() {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM: return false;
    case INTERP_KIND_NUCODE: return true;
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return false;
    }
}

bool interp_can_unsigned() {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM: return false;
    case INTERP_KIND_NUCODE: return true;
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return false;
    }
}

static AST *BCAllocaExpr(AST *siz) {
    if (gl_interp_kind != INTERP_KIND_P1ROM) {
        ERROR(siz, "Interpreter does not support __builtin_alloca");
        return siz;
    }
    AST *tmp = AstIdentifier("OUTB");
    AST *dcurr = AstIdentifier("__interp_dcurr");
    AST *assign = AstAssign(tmp, dcurr);
    AST *expr = AstOperator('+', siz, AstInteger(3));
    expr = AstOperator('&', expr, AstInteger(0xfffffffc));
    expr = FoldIfConst(expr);
    expr = AstOpAssign('+', dcurr, expr);
    expr = NewAST(AST_SEQUENCE, assign, expr);
    expr = NewAST(AST_SEQUENCE, expr, tmp);
    return expr;
}

static int BCGetNumResultsByType(AST *funcType) {
    int n = FuncNumResults(funcType);
    return (n<=1) ? 1 : n;
}

static int BCGetNumResults(Function *F) {
    int n = F->numresults;
    return (n<=1) ? 1 : n;
}

static int BCLocalSize(Function *F) {
    return FuncLocalSize(F); // F->numlocals*LONG_SIZE;
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
    } else if (ast->kind == AST_OBJECT) {
        // static reference has no size
        return 0;
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
    } else if (ast->kind == AST_OBJECT) {
        // Static reference to type, must have zero offset
        return 0;
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
    case INTERP_KIND_P1ROM: pbase_offset = 4*(ModData(P)->pub_cnt+ModData(P)->pri_cnt+ModData(P)->obj_cnt+1); break;
    default:
        if (printErrors) ERROR(errloc,"Unknown interpreter kind");
        return -1;
    }
    if (absolute) {
        int compiledAddress = ModData(P)->compiledAddress;
        if (compiledAddress < 0) {
            if (printErrors) ERROR(errloc,"Internal error, Taking address of uncompiled module");
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

// get HW register for extra return values
static int
HWRegRetval(int n) {
    if (gl_interp_kind == INTERP_KIND_P1ROM) {
        static const int reg_addr[] = { 0x1f5, 0x1f7, 0x1f3 };
        if (n < 1 || n > 3) {
            ERROR(NULL, "Return value index %d is out of range", n);
            return 0;
        }
        // return index for OUTB, DIRB, INB
        // these are $1f5, $1f7, $1f3 respectively
        return reg_addr[n-1] - 0x1e0;
    }
    ERROR(NULL, "Internal error, interpreter does not need return registers");
    return 0;
}

static int
HWRegVBase() {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM: {
        return 0x1EC - 0x1E0;
    } break;
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return 0;
    }
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
Optoken2MathOpKind(int token,bool *unaryOut,bool *needsAbsOut) {
    bool unary = false;
    bool needsAbs = false;
    
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

    case '<': mok = MOK_CMP_B; needsAbs = true; break;
    case '>': mok = MOK_CMP_A; needsAbs = true; break;
    case K_LE: mok = MOK_CMP_BE; needsAbs = true; break;
    case K_GE: mok = MOK_CMP_AE; needsAbs = true; break;
    case K_EQ: mok = MOK_CMP_E; needsAbs = true; break;
    case K_NE: mok = MOK_CMP_NE; needsAbs = true; break;

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
    if (needsAbsOut) {
        needsAbs = needsAbs && curfunc && LangBoolIsOne(curfunc->language);
        *needsAbsOut = needsAbs;
    }
    return mok;
}

// FIXME: less clunky name
// Basically returns true if the value will be the same when interpreted signed or unsigned
bool CanUseEitherSignedOrUnsigned(AST *node) {
    if (!node) return false;
    if (IsConstExpr(node)) return EvalConstExpr(node) >= 0;
    AST *type = ExprType(node);
    if (type) {
        if (type->kind == AST_UNSIGNEDTYPE && type->left->d.ival < LONG_SIZE) return true;
    }
    if (node->kind == AST_OPERATOR) {
        int32_t constVal;
        bool constRight = false;
        if (IsConstExpr(node->left)) constVal = EvalConstExpr(node->left);
        else if (IsConstExpr(node->right)) constVal = EvalConstExpr(node->right), constRight = true;
        else goto no_constop;

        if (node->d.ival == '&' && constVal >= 0) return true;
        if (node->d.ival == K_SHR && constRight && (constVal & 31)) return true;

    }
    no_constop:
    return false;
}

static bool OptNestedAdd(AST **node, int32_t *outVal) {
    if ((*node)->kind != AST_OPERATOR) return false;
    if (IsConstExpr(*node)) return false;
    if ((*node)->d.ival == '+' && IsConstExpr((*node)->left)) {
        *outVal = EvalConstExpr((*node)->left);
        *node = (*node)->right;
        return true;
    } else if ((*node)->d.ival == '+' && IsConstExpr((*node)->right)) {
        *outVal = EvalConstExpr((*node)->right);
        *node = (*node)->left;
        return true;
    } else if ((*node)->d.ival == '-' && IsConstExpr((*node)->right)) {
        *outVal = 0-EvalConstExpr((*node)->right);
        *node = (*node)->left;
        return true;
    }
    return false;
}

static bool isBoolOperator(int optoken) {
    switch (optoken) {
    case K_BOOL_AND:
    case K_BOOL_NOT:
    case K_BOOL_OR:
    case K_BOOL_XOR:
    case K_LOGIC_AND:
    case K_LOGIC_OR:
    case K_LOGIC_XOR:
        return true;
    default:
        return false;
    }
}

static bool OptimizeOperator(int *optoken, AST **left,AST **right) {
    ASTReportInfo save;
    int32_t addValue;

    // Try special cases first
    if (*optoken == K_SHL && left && IsConstExpr(*left) && EvalConstExpr(*left) == 1) {
        // 1<<x can be |<x
        *left = NULL;
        *optoken = K_DECODE;
        return true;
    }
    if (*optoken == K_ZEROEXTEND && right && IsConstExpr(*right)) {
        int32_t x = EvalConstExpr(*right);
        if (x >= 32) {
            x = -1; // TODO: Is this actually correct?
        } else {
            x = (1<<(x))-1;
        }
        *right = AstInteger(x);
        *optoken = '&';
    }
    if (*optoken == K_LIMITMIN && right && IsConstZero(*right) && left && CanUseEitherSignedOrUnsigned(*left)) {
        // Remove pointless limitmin
        *optoken = '+';
        *right = AstInteger(0);
    }
    // Handle nested add/sub
    try_addopt_again:
    if (*optoken == '+' && right && IsConstExpr(*right) && left && OptNestedAdd(left,&addValue)) {
        *right = AstInteger(EvalConstExpr(*right)+addValue);
        goto try_addopt_again;
    }
    if (*optoken == '+' && left && IsConstExpr(*left) && right && OptNestedAdd(right,&addValue)) {
        *left = AstInteger(EvalConstExpr(*left)+addValue);
        goto try_addopt_again;
    }
    if (*optoken == '-' && right && IsConstExpr(*right) && left && OptNestedAdd(left,&addValue)) {
        // (x+2) - 3 can be x - 1
        *right = AstInteger(EvalConstExpr(*right)-addValue);
        goto try_addopt_again;
    }
    if (*optoken == '-'  && left && IsConstExpr(*left) && right && OptNestedAdd(right,&addValue)) {
        // 3 - (x+2) can be 1 - x
        *left = AstInteger(EvalConstExpr(*left)-addValue);
        goto try_addopt_again;
    }
    // Handle x != 0 in boolean operators
    if (isBoolOperator(*optoken) && left && *left && (*left)->kind == AST_OPERATOR && (*left)->d.ival == K_NE) {
        if (IsConstZero((*left)->left)) *left = (*left)->right;
        else if (IsConstZero((*left)->right)) *left = (*left)->left;
    }
    if (isBoolOperator(*optoken) && right && *right && (*right)->kind == AST_OPERATOR && (*right)->d.ival == K_NE) {
        if (IsConstZero((*right)->left)) *right = (*right)->right;
        else if (IsConstZero((*right)->right)) *right = (*right)->left;
    }

    int shiftOptOp = 0;
    bool canNopOpt = false;
    int32_t nopOptVal = 0;
    bool canZeroOpt = false;
    int32_t zeroOptVal;
    bool canCommute = false;
    int commuteOp = 0;
    int negateOp = 0;
    int unsignedToSignedOp = 0;

    switch (*optoken) {
    case '*':
        canCommute = true;
        shiftOptOp = K_SHL;
        canNopOpt = true;
        nopOptVal = 1;
        canZeroOpt = true;
        zeroOptVal = 0;
        break;
    case '/':
        // Replacing with SAR doesn't work...
        canNopOpt = true;
        nopOptVal = 1;
        break;
    case K_UNS_DIV:
        shiftOptOp = K_SHR;
        unsignedToSignedOp = '/';
        break;
    case K_SHL:
    case K_SHR:
    case K_SAR:
    case K_ROTL:
    case K_ROTR:
        canNopOpt = true;
        nopOptVal = 0;
        break;
    case K_LIMITMAX_UNS:
        unsignedToSignedOp = K_LIMITMAX;
        break;
    case K_LIMITMIN_UNS:
        canNopOpt = true;
        nopOptVal = 0;
        unsignedToSignedOp = K_LIMITMIN;
        break;
    case K_LTU: unsignedToSignedOp = '<'; commuteOp = K_GTU; break;
    case K_GTU: unsignedToSignedOp = '>'; commuteOp = K_LTU; break;
    case K_LEU: unsignedToSignedOp = K_LE; commuteOp = K_GEU; break;
    case K_GEU: unsignedToSignedOp = K_GE; commuteOp = K_LEU; break;
    case '-':
        canNopOpt = true;
        nopOptVal = 0;
        negateOp = '+';
        break;
    case '+':
        canCommute = true;
        negateOp = '-';
        break;
    case '&':
        canCommute = true;
        canNopOpt = true;
        nopOptVal = -1;
        canZeroOpt = true;
        zeroOptVal = 0;
        break;
    case '|':
    case '^':
        canCommute = true;
        canNopOpt = true;
        nopOptVal = 0;
        break;
    case K_EQ:
    case K_NE:
        canCommute = true;
        break;
    default: return false;
    }

    if (commuteOp) canCommute = true;
    else if (canCommute && !commuteOp) commuteOp = *optoken;
    // Try commuting constant to the right (mostly just makes the code simpler)
    if (canCommute && left && right && IsConstExpr(*left) && !IsConstExpr(*right)) {
        int uncommutedOp = *optoken;
        AST *swap = *left;
        *left = *right;
        *right = swap;
        *optoken = commuteOp;
        if (OptimizeOperator(optoken,left,right)) return true;
        // Undo swap if we couldn't do anything (fixes "waitcnt(381+cnt)")
        // TODO: Can we only do this when actually neccessary?
        //       Because having the constant on the right is better for IR-level optimization (write+read contract)
        swap = *left;
        *left = *right;
        *right = swap;
        *optoken = uncommutedOp;
    }

    if (right && *right && IsConstExpr(*right)) {
        int32_t rightVal = EvalConstExpr(*right);

        if (left && canZeroOpt && rightVal == zeroOptVal && *left && !ExprHasSideEffects(*left)) {
            AstReportAs(*right,&save);
            *optoken = '+';
            *left = AstInteger(0);
            *right = AstInteger(0);
            AstReportDone(&save);
            return true;
        } else if (canNopOpt && rightVal == nopOptVal) {
            if (left && *left && (*left)->kind == AST_OPERATOR) {
                *optoken = (*left)->d.ival;
                *right = (*left)->right;
                *left = (*left)->left;
                OptimizeOperator(optoken,left,right);
                return true;
            } else {
                AstReportAs(*right,&save);
                *optoken = '+';
                *right = AstInteger(0);
                AstReportDone(&save);
                return true;
            }
        } else if (shiftOptOp && isPowerOf2(rightVal)) {
            AstReportAs(*right,&save);
            *optoken = shiftOptOp;
            *right = AstInteger(31-__builtin_clz(rightVal));
            AstReportDone(&save);
            return true;
        } else if (negateOp && rightVal < 0 && rightVal != INT32_MIN) {
            AstReportAs(*right,&save);
            *optoken = negateOp;
            *right = AstInteger(-rightVal);
            AstReportDone(&save);
            return true;
        } else if (*optoken == K_GTU && rightVal == 0) {
            *optoken = K_NE;
            return true;
        } else if (*optoken == K_EQ && rightVal == 0) {
            *optoken = K_BOOL_NOT;
            *right = *left;
            *left = NULL;
            return true;
        }
    } else if (right) {
        if (negateOp && (*right)->kind == AST_OPERATOR && (*right)->d.ival == K_NEGATE) {
            // L + (-R) can be L - R
            *right = (*right)->right;
            *optoken = negateOp;
            OptimizeOperator(optoken,left,right);
            return true;
        }
    }

    if (unsignedToSignedOp && !interp_can_unsigned() && left && right && CanUseEitherSignedOrUnsigned(*left) && CanUseEitherSignedOrUnsigned(*right)) {
        *optoken = unsignedToSignedOp;
        return true;
    }

    return false;
}

ByteOpIR BCBuildString(AST *expr) {
    ByteOpIR ir = {0};
    ir.kind = BOK_FUNDATA_STRING;
    Flexbuf fb;
    flexbuf_init(&fb, 32);
    StringBuildBuffer(&fb, expr);
    ir.attr.stringLength = flexbuf_curlen(&fb);
    ir.data.stringPtr = flexbuf_get(&fb);
    return ir;
}

#ifdef NEED_ORIG_DEBUG
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
#else
#define printASTInfo(node) DumpAST(node)
#endif

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

static ByteOpIR*
BCNewNamedLabelRef(BCContext context,const char *name) {
    ByteOpIR *lbl = calloc(sizeof(ByteOpIR),1);
    lbl->kind = BOK_NAMEDLABEL;
    lbl->attr.labelHiddenVars = context.hiddenVariables;
    lbl->data.stringPtr = name;
    return lbl;
}

static void
BCCompileInteger(BCIRBuffer *irbuf,int32_t ival) {
    ByteOpIR opc = {0};
    opc.kind = BOK_CONSTANT;
    opc.data.int32 = ival;
    BIRB_PushCopy(irbuf,&opc);
}

static void
BCCompilePushModuleFuncRef(BCIRBuffer *irbuf,Module *M,int32_t funcid)
{
    ByteOpIR opc = {0};
    opc.kind = BOK_CONSTANT_FUNCREF;
    opc.attr.funcval.modref = M;
    opc.data.int32 = funcid;
    BIRB_PushCopy(irbuf,&opc);
}

static void
BCCompilePushModuleDatRef(BCIRBuffer *irbuf,Module *M,int32_t offset)
{
    ByteOpIR opc = {0};
    opc.kind = BOK_CONSTANT_DATREF;
    opc.attr.datval.modref = M;
    opc.data.int32 = offset;
    BIRB_PushCopy(irbuf,&opc);
}

static void
BCCompileDatModuleFuncRef(uint8_t *where,Module *M,int32_t funcid)
{
    ERROR(NULL, "Unable to put data reloc");
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
BCCompileJumpEx(BCIRBuffer *irbuf, ByteOpIR *label, enum ByteOpKind kind, BCContext context, int stackoffset, bool logicallyTerminal) {
    int stackdiff = context.hiddenVariables - label->attr.labelHiddenVars + stackoffset; // Will be zero if we got an AST_NAMEDLABEL
    if (stackdiff > 0) {
        // emit pop to get rid of hidden vars
        if (kind != BOK_JUMP) ERROR(NULL,"Internal Error: Compiling conditional jump to label with less hidden vars than current context");
        else BCCompilePopN(irbuf,stackdiff);
    } else if (stackdiff < 0) {
        ERROR(NULL,"Internal Error: Compiling jump to label with more hidden vars than current context");
    }
    ByteOpIR condjmp = {0};
    condjmp.kind = kind;
    condjmp.jumpTo = label;
    condjmp.attr.condjump.logicallyTerminal = logicallyTerminal;
    BIRB_PushCopy(irbuf,&condjmp);
}

static void
BCCompileJump(BCIRBuffer *irbuf, ByteOpIR *label, BCContext context) {
    BCCompileJumpEx(irbuf,label,BOK_JUMP,context,0,false);
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
    } else if (condition->kind == AST_OPERATOR) {
        int optoken = condition->d.ival;
        AST *left = condition->left, *right = condition->right;
        bool extrasmall = curfunc->optimize_flags & OPT_EXTRASMALL;

        OptimizeOperator(&optoken,&left,&right);

        if (optoken == K_BOOL_NOT) {
            // Inverted jump
            BCCompileConditionalJump(irbuf,right,!ifNotZero,label,context);
            return;
        } else if ((optoken == K_EQ || optoken == K_NE ) 
        && ( IsConstZero(left) || IsConstZero(right))) {
            // Slightly complex condition, I know
            // optimize conditions like x == 0 and x<>0
            BCCompileConditionalJump(irbuf,IsConstZero(left) ? right : left,!!ifNotZero != !!(optoken == K_EQ),label,context);
            return;
        } else if ((optoken == K_BOOL_AND || optoken == K_BOOL_OR) && extrasmall && !ExprHasSideEffects(right)) {
            goto normal_condjump; // using an AND/OR operator is smaller but slower
        } else if (optoken == K_BOOL_AND && !ifNotZero) {
            // like in "IF L AND R"
            BCCompileConditionalJump(irbuf,left,false,label,context);
            BCCompileConditionalJump(irbuf,right,false,label,context);
            return;
        } else if (optoken == K_BOOL_AND && ifNotZero) {
            // like in "IFNOT L AND R"
            ByteOpIR *skipLabel = BCNewOrphanLabel(context);
            BCCompileConditionalJump(irbuf,left,false,skipLabel,context);
            BCCompileConditionalJump(irbuf,right,true,label,context);
            BIRB_Push(irbuf,skipLabel);
            return;
        } else if (optoken == K_BOOL_OR && !ifNotZero) {
            // like in "IF L OR R"
            ByteOpIR *skipLabel = BCNewOrphanLabel(context);
            BCCompileConditionalJump(irbuf,left,true,skipLabel,context);
            BCCompileConditionalJump(irbuf,right,false,label,context);
            BIRB_Push(irbuf,skipLabel);
            return;
        } else if (optoken == K_BOOL_OR && ifNotZero) {
            // like in "IFNOT L OR R"
            BCCompileConditionalJump(irbuf,left,true,label,context);
            BCCompileConditionalJump(irbuf,right,true,label,context);
            return;
        }
        goto normal_condjump;
    } else {
        normal_condjump:
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

    AST *type = ExprType(node);
    AST *typeoverride = NULL;
    Symbol *sym = NULL;
    HwReg *hwreg;
    AST *baseExpr = NULL;
    AST *indexExpr = NULL;
    AST *typeAfterIndex = NULL;
    AST *bitExpr1 = NULL;
    AST *bitExpr2 = NULL;
    AST *ident;
    int memberOffset = 0;
    int pushMultiple = 0;
    
    if (node->kind == AST_METHODREF) {
        AST *selector = node->right;
        Module *P = GetClassPtr(ExprType(node->left));
        if (!P) {
            ERROR(node,"Unable to find object");
            return;
        }
        if (!IsIdentifier(selector)) {
            ERROR(node,"Expected identifier after `.'");
            return;
        }
        const char *memberName = GetUserIdentifierName(selector);
        node = node->left;
        sym = LookupSymbolInTable(&P->objsyms, memberName);
        if (!sym) {
            ERROR(node,"Unable to find member %s in class %s", memberName, P->classname);
            return;
        }
        switch(sym->kind) {
        case SYM_VARIABLE:
            memberOffset += sym->offset;
            type = sym->val;
            break;
        case SYM_FUNCTION:
            if (kind == MEMOP_ADDRESS) {
                Function *F = (Function *)sym->val;
                Module *M = F->module;
                if (M->bedata == NULL) {
                    ERROR(node, "Cannot find address for function %s (no module info)",  sym->user_name);
                    return;
                }
                int32_t addr = ModData(M)->compiledAddress;
                int id = getFuncIDForKnownFunc(M, F);
                if (addr < 0) {
                    //WARNING(node, "Need to patch address later");
                    BCCompilePushModuleFuncRef(irbuf, M, id);
                } else {
                    uint32_t val = (id<<16) | addr;
                    BCCompileInteger(irbuf, val);
                }
                return;
            }
            ERROR(node, "Unhandled memory operation on function");
            break;
        case SYM_LABEL:
        {
            Module *M = sym->module;
            if (!M) {
                ERROR(node,"Method reference for unknown module");
                return;
            }
            if (M->bedata == NULL) {
                ERROR(node, "Cannot find address for field %s (no module info)",  sym->user_name);
                return;
            }
            Label *lab = (Label *)sym->val;
            BCCompilePushModuleDatRef(irbuf, M, lab->hubval);
            switch (kind) {
            case MEMOP_ADDRESS:  break; /* nothing more to do */
            case MEMOP_READ:
            case MEMOP_WRITE:
                break;
                type = lab->type;
                typeoverride = BaseType(type);
                targetKind = MOT_MEM;
                memOp.attr.memop.base = MEMOP_BASE_POP;
                baseExpr = NULL;
                memberOffset = 0;
                goto nosymbol_memref;
            default:
                ERROR(node, "Unsupported operation on object DAT field");
                break;
            }
            return;
        }
        default:
            ERROR(node,"Wrong kind of symbol (%d) in method reference", sym->kind);
            break;
        }
    }
    if (node->kind == AST_ARRAYREF) {
        ident = node->left;
        indexExpr = node->right;
        typeAfterIndex = BaseType(ExprType(ident));
    } else ident = node;

    try_ident_again:
    if (IsIdentifier(ident) || ident->kind == AST_SYMBOL) sym = LookupAstSymbol(ident,NULL);
    else if (ident->kind == AST_RESULT) {
        if (curfunc && curfunc->resultexpr && IsIdentifier(curfunc->resultexpr)) {
            sym = LookupAstSymbol(curfunc->resultexpr, NULL);
        } else {
            sym = LookupSymbol("result");
            if (!sym) {
                WARNING(node, "unable to find result variable");
                sym = calloc(sizeof(*sym), 1);
                sym->kind = SYM_RESULT;
            }
        }
    }
    else if (ident->kind == AST_MEMREF) {
        if (!typeoverride && ident->right->kind == AST_ADDROF && 
        ident->right->left && ident->right->left->kind == AST_ARRAYREF && (!indexExpr || IsConstZero(indexExpr) || IsConstZero(ident->right->left->right))) {
            // Handle weird AST representation of "var.byte[x]""
            DEBUG(node,"handling size override kind 1...");
            typeoverride = ident->left;
            if (!IsConstZero(ident->right->left->right)) indexExpr = ident->right->left->right;
            ident = ident->right->left->left;
            goto try_ident_again;
        } else if (!typeoverride && ident->right->kind == AST_ADDROF && IsIdentifier(ident->right->left)) {
            DEBUG(node,"handling size override kind 2...");
            typeoverride = ident->left;
            ident = ident->right->left;
            goto try_ident_again;
        } else {
            // normal raw memory access
            memOp.attr.memop.base = MEMOP_BASE_POP;
            targetKind = MOT_MEM;

            if (type) typeoverride = type;
            type = ident->left;
            baseExpr = ident->right;
            if (memberOffset) {
                baseExpr = AstOperator('+', baseExpr, AstInteger(memberOffset));
                memberOffset = 0;
            }
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
        if (node->kind == AST_ARRAYREF && indexExpr && !baseExpr) {
            memOp.attr.memop.base = MEMOP_BASE_POP;
            targetKind = MOT_MEM;
            
            baseExpr = ident;
            goto nosymbol_memref;
        }
        ERROR(ident,"Can't get symbol");
        return;
    } else {
        //printf("got symbol with name %s and kind %d\n",sym->our_name,sym->kind);
        if (!type) type = ExprType(ident);
        switch (sym->kind) {
        case SYM_LABEL: {
            memOp.attr.memop.base = MEMOP_BASE_PBASE;
            targetKind = MOT_MEM;

            Label *lab = sym->val;
            uint32_t labelval = lab->hubval;
            // Add header offset
            if (sym->module == current || sym->module == 0) {
                labelval += BCgetDAToffset(current,false,node,true);
                memOp.data.int32 = labelval;
            } else {
                WARNING(ident, "bytecode may not always support cross-module label references");
                labelval += BCgetDAToffset(sym->module, true,node,true);
                memOp.attr.memop.base = MEMOP_BASE_POP;
                if (baseExpr) ERROR(node,"baseExpr already set?!?!");
                else baseExpr = AstInteger(labelval);
            }
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
        case SYM_CLOSURE:{
            BCAddStaticReference(current, (AST *)sym->val);
            memOp.attr.memop.base = MEMOP_BASE_VBASE;
            targetKind = MOT_MEM;
            memOp.data.int32 = 0;
        } break;
        case SYM_LOCALVAR: {
            memOp.attr.memop.base = (curfunc->closure) ? MEMOP_BASE_VBASE : MEMOP_BASE_DBASE;
            targetKind = MOT_MEM;
            memOp.data.int32 = sym->offset;
        } break;
        case SYM_PARAMETER: {
            memOp.attr.memop.base = (curfunc->closure) ? MEMOP_BASE_VBASE : MEMOP_BASE_DBASE;
            targetKind = MOT_MEM;
            memOp.data.int32 = sym->offset;
        } break;
        case SYM_TEMPVAR: {
            memOp.attr.memop.base = MEMOP_BASE_DBASE;
            targetKind = MOT_MEM;
            memOp.data.int32 = sym->offset;
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
            memOp.data.int32 = sym->offset;
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
        case SYM_LOCALLABEL: {
            if (kind == MEMOP_ADDRESS) {
                if (indexExpr || baseExpr) {
                    ERROR(node,"Unable to offset label");
                }
                ByteOpIR *labelRef = BCNewNamedLabelRef(context,sym->our_name);
                ByteOpIR pushOp = {.kind = BOK_FUNDATA_PUSHADDRESS, .jumpTo = labelRef, .attr.pushaddress.addPbase = true};
                BIRB_PushCopy(irbuf, &pushOp);
                return;
            }
            ERROR(node,"Unhandled memory operation on local label");
        } break;
        case SYM_FUNCTION: {
            if (kind == MEMOP_ADDRESS) {
                Function *F = (Function *)sym->val;
                Module *M = F->module;
                if (M->bedata == NULL) {
                    ERROR(node, "Cannot find address for function (no module info)");
                    return;
                }
                int32_t addr = ModData(M)->compiledAddress;
                int id = getFuncIDForKnownFunc(M, F);
                if (addr < 0) {
                    //WARNING(node, "Need to patch address later");
                    BCCompilePushModuleFuncRef(irbuf, M, id);
                } else {
                    uint32_t val = (id<<16) | (addr);
                    BCCompileInteger(irbuf, val);
                }
                return;
            }
            ERROR(node, "Unhandled memory operation on function");
        } break;
        case SYM_ALIAS: {
            AST *expr = (AST *)sym->val;
            AST *typ = NULL;
            if (expr->kind == AST_CAST) {
                typ = expr->left;
                expr = expr->right;
            }
            if (typ) typeoverride = type;
            if (expr && expr->kind == AST_ARRAYREF) {
                expr = expr->left;
            }
            if (IsIdentifier(expr) || expr->kind == AST_MEMREF) {
                ident = expr;
                goto try_ident_again;
            }
            ERROR(ident,"DECLARE ALIAS %s not handled yet in bytecode", sym->user_name);
            return;
        }
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
    
    if (!type) type = ast_type_long;

    switch (type->kind) {
    case AST_GENERICTYPE:
    case AST_UNSIGNEDTYPE: {
        int size = type->left->d.ival;
        switch (size) {
        case 1: memOp.attr.memop.memSize = MEMOP_SIZE_BYTE; break;
        case 2: memOp.attr.memop.memSize = MEMOP_SIZE_WORD; break;
        // Technically treated as signed, but all the unsigned operators are seperate, anyways, so I guess it's fine?
        case 4: memOp.attr.memop.memSize = MEMOP_SIZE_LONG; break;
        case 8: memOp.attr.memop.memSize = MEMOP_SIZE_LONG; pushMultiple = 2; break;
        default: ERROR(node,"Can't handle unsigned type with size %d",size); break;
        }
    } break;
    case AST_INTTYPE: {
        int size = type->left->d.ival;
        switch (size) {
        // sign-extend is generated for these
        case 1: if (kind == MEMOP_WRITE || kind == MEMOP_READ) memOp.attr.memop.memSize = MEMOP_SIZE_BYTE; else goto signed_todo; break;
        case 2: if (kind == MEMOP_WRITE || kind == MEMOP_READ) memOp.attr.memop.memSize = MEMOP_SIZE_WORD; else goto signed_todo; break;
        case 4: memOp.attr.memop.memSize = MEMOP_SIZE_LONG; break; 
        case 8: memOp.attr.memop.memSize = MEMOP_SIZE_LONG; pushMultiple = 2; break;
        signed_todo:
        default: ERROR(node,"Can't handle signed type with size %d",size); break;
        }
    } break;
    case AST_FLOATTYPE: {
        int size = type->left->d.ival;
        switch (size) {
        case 4: memOp.attr.memop.memSize = MEMOP_SIZE_LONG; break; 
        case 8: memOp.attr.memop.memSize = MEMOP_SIZE_LONG; pushMultiple = 2; break;
        default: ERROR(node,"Can't handle float type with size %d",size); break;
        }
    } break;
    case AST_PTRTYPE:
    case AST_FUNCTYPE: {
        memOp.attr.memop.memSize = MEMOP_SIZE_LONG;
    } break;
    case AST_OBJECT: {
        int size = TypeSize(type);
        if (size == 4) {
            memOp.attr.memop.memSize = MEMOP_SIZE_LONG;
        } else if (kind == MEMOP_ADDRESS) {
            // we're creating a pointer, need to offset appropriately
            if (indexExpr) {
                indexExpr = AstOperator('*', indexExpr, AstInteger(size / LONG_SIZE));
            }
            memOp.attr.memop.memSize = MEMOP_SIZE_LONG;
        } else {
            // need to push multiple values here
            memOp.attr.memop.memSize = MEMOP_SIZE_LONG;
            pushMultiple = size / 4;
        }
    } break;
    default:
        ERROR(node,"Unhandled type kind %d",type->kind);
    }

    after_typeinfer:

    memOp.attr.memop.modSize = memOp.attr.memop.memSize; // Let's just assume these are the same

    if (indexExpr && TypeSize(typeAfterIndex) != TypeSize(type)) {
        indexExpr = AstOperator('*', indexExpr, AstInteger(TypeSize(typeAfterIndex) / TypeSize(type)));
        type = typeAfterIndex;
    }
    switch(targetKind) {
    case MOT_MEM: {

        if (memberOffset) {
            memOp.data.int32 += memberOffset;
            if (memOp.attr.memop.base == MEMOP_BASE_POP) ERROR(node,"Internal Error: memberOffset on POP base");
        }

        // transform long[a+b] to long[a][b/4]
        if (baseExpr && (!indexExpr || IsConstZero(indexExpr)) && baseExpr->kind == AST_OPERATOR && baseExpr->d.ival == '+') {

            if (memOp.attr.memop.memSize == MEMOP_SIZE_BYTE) {
                // Can always do this transform for bytes
                indexExpr = baseExpr->right;
                baseExpr = baseExpr->left;
            } else {
                int typeShift = memOp.attr.memop.memSize == MEMOP_SIZE_WORD ? 1 : 2;
                int typeMask  = memOp.attr.memop.memSize == MEMOP_SIZE_WORD ? 1 : 3;
                if (IsConstExpr(baseExpr->right) && (EvalConstExpr(baseExpr->right) & typeMask) == 0) {
                    indexExpr = AstInteger(EvalConstExpr(baseExpr->right) >> typeShift);
                    baseExpr = baseExpr->left;
                } else if (IsConstExpr(baseExpr->left) && (EvalConstExpr(baseExpr->left) & typeMask) == 0) {
                    indexExpr = AstInteger(EvalConstExpr(baseExpr->left) >> typeShift);
                    baseExpr = baseExpr->right;
                }
            }
        }

        //if (!!baseExpr != !!(memOp.attr.memop.base == MEMOP_BASE_POP)) ERROR(node,"Internal Error: baseExpr condition mismatch");
        if (bitExpr1) ERROR(node,"Bit1 expression on memory op!");
        if (bitExpr2) ERROR(node,"Bit2 expression on memory op!");

        if (baseExpr) {
            if (baseExpr->kind == AST_METHODREF) {
                baseExpr = NewAST(AST_ADDROF, baseExpr, NULL);
            }
            BCCompileExpression(irbuf,baseExpr,context,false);
        }

        // handle index
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
            } else if (baseExpr && pushMultiple) {
                ERROR(indexExpr, "Cannot handle indices with large values");
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

    if (pushMultiple && memOp.attr.memop.memSize == MEMOP_SIZE_LONG) {
        if (kind == MEMOP_READ) {
            // push in regular order
            for (int i = 0; i < pushMultiple; i++) {
                if (i > 0 && baseExpr) {
                    AST *newbase = AstOperator('+', baseExpr, AstInteger(i*LONG_SIZE));
                    BCCompileExpression(irbuf,newbase,context,false);
                }
                BIRB_PushCopy(irbuf,&memOp);
                memOp.data.int32 += LONG_SIZE;
            }
        } else if (kind == MEMOP_WRITE && 0) {
            // pop in reverse order
        } else {
            ERROR(node,"Cannot handle large objects yet");
        }
    } else {
        BIRB_PushCopy(irbuf,&memOp);
    }
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
BCPopRestOfList(BCIRBuffer *irbuf,AST*list,BCContext context)
{
    if (!list) return;
    BCPopRestOfList(irbuf,list->right,context);
    BCCompileMemOp(irbuf,list->left,context,MEMOP_WRITE);
}

static void
BCCompileAssignment(BCIRBuffer *irbuf,AST *node,BCContext context,bool asExpression,enum MathOpKind modifyMathKind) {
    ASSERT_AST_KIND(node,AST_ASSIGN,return;);
    AST *left = node->left, *right = node->right;
    AST *type;
    bool modifyReverseMath = false;
    bool needsAbs = false;
    
    // check for left hand side needing multiple assignments
    if (left->kind != AST_EXPRLIST) {
        int size = LONG_SIZE;
        type = ExprType(left);
        if (type) size = TypeSize(type);
        if (size > LONG_SIZE) {
            ASTReportInfo save;
            AST *list = NULL;
            AST *elem;
            size = (size + LONG_SIZE-1)/LONG_SIZE;
            AstReportAs(left,&save);
            // convert to a list of assignments
            // like long[@foo][n] = ...
            AST *ref = NewAST(AST_ADDROF, left, NULL);
            ref = NewAST(AST_MEMREF, ast_type_long, ref);
            for (int i = 0; i < size; i++) {
                elem = NewAST(AST_ARRAYREF, ref, AstInteger(i));
                list = AddToList(list, NewAST(AST_EXPRLIST, elem, NULL));
            }
            AstReportDone(&save);
            left = list;
        }
    }
    // This only happens with assignments where the lhs has side effects
    if (node->d.ival != K_ASSIGN) {
        if (modifyMathKind) ERROR(node,"direct operator AND given modfiyMathKind in AST_ASSIGN is unhandled");
        if (left->kind == AST_EXPRLIST) ERROR(node,"multiple assignment with side effects is unhandled");
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
            mok = Optoken2MathOpKind(optoken,&isUnary,&needsAbs);
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
                }
            }
            break;
        default: mok = Optoken2MathOpKind(optoken,&isUnary,&needsAbs); break;
        }
        if (mok == 0 || needsAbs) goto nocontract; // Can't handle this operator

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

redoLeft:
    switch(left->kind) {
    case AST_EMPTY:
        if (asExpression || ExprHasSideEffects(right)) BCCompileExpression(irbuf,right,context,!asExpression);
        return;
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
    case AST_METHODREF: {
        memopNode = left;
    } break;
    case AST_SYMBOL:
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER: {
        Symbol *sym = LookupAstSymbol(left,NULL);
        if (sym) {
            switch (sym->kind) {
            case SYM_TEMPVAR: DEBUG(node,"temp variable %s used",sym->our_name); // fall through
            case SYM_LABEL:
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
            case SYM_HWREG:
                memopNode = NewAST(AST_HWREG, NULL, NULL);
                memopNode->d.ptr = sym->val;
                break;
            case SYM_ALIAS: {
                // this is an alias for an expression, so compile the expression
                AST *expr = (AST *)sym->val;
                while (expr && expr->kind == AST_CAST) {
                    expr = expr->right;
                }
                if (!expr || !IsIdentifier(expr)) {
                    ERROR(left, "Non-identifier ALIAS in assignments not handled yet");
                    return;
                }
                left = expr;
                goto redoLeft;
            }
            default:
                ERROR(left,"Unhandled Identifier symbol kind %d in assignment",sym->kind);
                return;
            }
        } else {
            ERROR(node,"Internal Error: no symbol in identifier assign");
            return;
        }
    } break;
    case AST_EXPRLIST: {
        if (asExpression) {
            ERROR(left, "Multiple assignment is valid only at top level");
            return;
        }
        BCCompileExpression(irbuf, right, context, false);
        // we have to pop the values off in the reverse order
        BCPopRestOfList(irbuf, left, context);
        return;
    } break;
    default:
        ERROR(left,"Unhandled assign left kind %d",left->kind);
        break;
    }

    if (!memopNode) {
        ERROR(node,"Internal error, no memopNode");
        return;
    }

    

    if (modifyMathKind == 0 || needsAbs) {
        if (asExpression) ERROR(node,"Internal Error: shouldn't happen");
        BCCompileExpression(irbuf,right,context,false);
        BCCompileMemOp(irbuf,memopNode,context,MEMOP_WRITE);
    } else {
        if (!isUnaryModify) BCCompileExpression(irbuf,right,context,false);
        BCCompileMemOpEx(irbuf,memopNode,context,MEMOP_MODIFY,modifyMathKind,modifyReverseMath,asExpression);
    }
}

static int getFuncID(Module *M,const char *name) {
    int (*StrComp)(const char *, const char *);
    
    if (!M->bedata) {
        ERROR(NULL,"Internal Error: bedata empty");
        return -1;
    }
    if ( (curfunc && LangCaseSensitive(curfunc->language)) ||
         LangCaseSensitive(M->curLanguage)) {
        StrComp = strcmp;
    } else {
        StrComp = strcasecmp;
    }
    for (int i=0;i<ModData(M)->pub_cnt;i++) {
        if (!StrComp(ModData(M)->pubs[i]->name,name)) return i + 1;
    }
    for (int i=0;i<ModData(M)->pri_cnt;i++) {
        if (!StrComp(ModData(M)->pris[i]->name,name)) return ModData(M)->pub_cnt + i + 1;
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

Function *BCgetFuncForId(Module *M, int id) {
    if (!M) return NULL;
    if (!M->bedata) {
        ERROR(NULL,"Internal Error: bedata empty");
        return NULL;
    }
    if (id == 0) return NULL;
    else if (id<=(ModData(M)->pub_cnt)) return ModData(M)->pubs[id-1];
    else if (id<=(ModData(M)->pub_cnt+ModData(M)->pri_cnt)) return ModData(M)->pris[id-(1+ModData(M)->pub_cnt)];
    else return NULL;
}

const char *BCgetFuncNameForId(Module *M, int id) {
    Function *F = BCgetFuncForId(M,id);
    return F ? F->name : "!!INVALID!!";
}

// FIXME this seems very convoluted
static int getObjID(Module *M,const char *name, AST** gettype) {
    if (!M->bedata) {
        ERROR(NULL,"Internal Error: bedata empty");
        return -1;
    }
    for(int i=0;i<ModData(M)->obj_cnt;i++) {
        AST *obj = ModData(M)->objs[i];
        if (obj->kind != AST_DECLARE_VAR) continue;
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
static int getObjIDByClass(Module *M, AST *class, bool mustBeStatic, AST** gettype) {
    Module *classptr = GetClassPtr(class);
    if (!classptr) return 0;
    if (!M->bedata) {
        ERROR(NULL,"Internal Error: bedata empty");
        return -1;
    }
    for(int i=0;i<ModData(M)->obj_cnt;i++) {
        AST *obj = ModData(M)->objs[i];
        if (obj->kind == AST_DECLARE_VAR) {
            if (mustBeStatic) continue;
            ASSERT_AST_KIND(obj->left,AST_OBJECT,return 0;);
            ASSERT_AST_KIND(obj->right,AST_LISTHOLDER,return 0;);
            AST *ident = obj->right->left;
            Module *ptr = GetClassPtr(obj->left);
            ASSERT_AST_KIND(ident,AST_IDENTIFIER,return 0;);
            if (ptr != classptr) continue; 
            if (gettype) *gettype = obj->left;
            return i+1+ModData(M)->pub_cnt+ModData(M)->pri_cnt;
        } else {
            Module *ptr = GetClassPtr(obj);
            if (ptr != classptr) continue; 
            if (gettype) *gettype = obj;
            return i+1+ModData(M)->pub_cnt+ModData(M)->pri_cnt;
        }
    }
    ERROR(class,"can't find OBJ id for class %s",classptr->classname);
    return 0;
}

Module *BCgetModuleForOBJID(Module *M,int id) {
    if (!M->bedata) {
        ERROR(NULL,"Internal Error: bedata empty");
        return NULL;
    }
    id -= 1+ModData(M)->pub_cnt+ModData(M)->pri_cnt;
    if (id<0) return NULL;
    AST *obj = ModData(M)->objs[id];
    if (obj->kind == AST_DECLARE_VAR) {
        ASSERT_AST_KIND(obj->left,AST_OBJECT,return NULL;);
        return GetClassPtr(obj->left);
    } else if (obj->kind == AST_OBJECT) {
        return GetClassPtr(obj);
    } else {
        ERROR(obj,"Internal Error: Unhandled node kind %d in BCgetModuleForOBJID",obj->kind);
        return NULL;
    }
}

// only really for display purposes
const char *BCgetNameForOBJID(Module *M,int id) {
    if (!M->bedata) {
        ERROR(NULL,"Internal Error: bedata empty");
        return "!ERROR!";
    }
    id -= 1+ModData(M)->pub_cnt+ModData(M)->pri_cnt;
    if (id<0) return "!INVALID!";
    AST *obj = ModData(M)->objs[id];
    if (obj->kind == AST_DECLARE_VAR) {
        ASSERT_AST_KIND(obj->right,AST_LISTHOLDER,return NULL;);
        AST *ident = obj->right->left;
        if (ident && ident->kind == AST_ARRAYDECL) ident = ident->left;
        ASSERT_AST_KIND(ident,AST_IDENTIFIER,return NULL;);
        return GetIdentifierName(ident);
    } else if (obj->kind == AST_OBJECT) {
        return GetClassPtr(obj)->classname;
    } else {
        ERROR(obj,"Internal Error: Unhandled node kind %d in BCgetNameForOBJID",obj->kind);
        return NULL;
    }
}

static void
BCCompileFunCall(BCIRBuffer *irbuf,AST *node,BCContext context, bool asExpression, bool rescueAbort) {

    Symbol *sym = NULL;
    int callobjid = -1;
    int extraResults = 0;
    AST *objtype = NULL;
    AST *vbase_expr = NULL;
    AST *objindex_expr = NULL;
    AST *indirect_call_expr = NULL;
    bool dupFirstParameter = false;
    
    ByteOpIR callOp = {0};
    ByteOpIR anchorOp = {0};
    anchorOp.kind = BOK_ANCHOR;
    anchorOp.attr.anchor.withResult = asExpression;
    anchorOp.attr.anchor.rescue = rescueAbort;

    if (node->left && node->left->kind == AST_METHODREF) {
        AST *ident = node->left->left;
        if (ident->kind == AST_ARRAYREF) {
            objindex_expr = ident->right;
            ident = ident->left;
        }
        if (ident->kind == AST_CAST) {
            // assume this must be a call to a
            // static method and use any available object
            // (this may not actually always work, but it's enough to
            // get us going)
            callobjid = getObjIDByClass(current, ident->left, false, &objtype);
        } else if (ident->kind == AST_MEMREF) {
            callobjid = getObjIDByClass(current, ident->left, true, &objtype);
            vbase_expr = ident->right;
            // This only seems to occur inside of AST_ARRAYREF. Let's hope the index is never not zero
        } else {
            if (!IsIdentifier(ident)) {
                ASSERT_AST_KIND(ident,AST_IDENTIFIER,return;);
            }
            Symbol *objsym = LookupAstSymbol(ident,NULL);
            if (!objsym) {
                ERROR(node->left,"Internal error, Null call receiver");
                return;
            } else if (objsym->kind == SYM_VARIABLE) {
                // Normal call
                callobjid = getObjID(current,objsym->our_name,&objtype);
            } else {
                DEBUG(node->left,"Converting call with receiver symbol kind %d to pointer call",objsym->kind);
                ASTReportInfo save;
                AST *type = ExprType(ident);
                callobjid = getObjIDByClass(current, type, true, &objtype);
                AstReportAs(ident,&save);
                vbase_expr = NewAST(AST_ADDROF,ident,NULL);
                AstReportDone(&save);
            }           
        }
        if (callobjid<0) {
            ERROR(node->left,"Not an OBJ of this object");
            return;
        }
        if (vbase_expr && objindex_expr && !IsConstZero(objindex_expr)) WARNING(node,"Call through pointer with non-zero index!");
        if (objindex_expr && IsConstExpr(objindex_expr)) {
            callobjid += EvalConstExpr(objindex_expr);
            objindex_expr = NULL;
        }

        const char *thename = GetIdentifierName(node->left->right);
        if (!thename) {
            return;
        }
        
        sym = LookupMemberSymbol(node, objtype, thename, NULL, NULL);
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
            // not an identifier: assume a method call
            // ERROR(node, "Function call is not an identifier");
            goto indirect_call;
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

    if (!sym) {
        if (IsIdentifier(node->left)) {
            const char *name = GetUserIdentifierName(node->left);
            ERROR(node,"Function %s not found", name);
        } else if (IsIdentifier(node)) {
            const char *name = GetUserIdentifierName(node);
            ERROR(node,"Identifier %s not found", name);
        } else {
            ERROR(node,"Function call has no symbol");
        }
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
            } else if (!strcmp(sym->our_name,"bytemove") || !strcmp(sym->our_name, "__builtin_memcpy")) {
                // need to manually return the first parameter
                // (the interpreter doesn't do this for us)
                callOp.kind = BOK_BUILTIN_BULKMEM;
                callOp.attr.bulkmem.isMove = true;
                callOp.attr.bulkmem.memSize = BULKMEM_SIZE_BYTE;
                dupFirstParameter = true;
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
            } else if (!strcmp(sym->our_name,"bytemove") || !strcmp(sym->our_name, "__builtin_memcpy")) {
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
        Function *func = (Function *)sym->val;
        int funid = getFuncID(objtype ? GetClassPtr(objtype) : current, sym->our_name);
        if (funid < 0) {
            ERROR(node,"Can't get function id for %s",sym->our_name);
            return;
        }
        if (callobjid<0) {
            callOp.kind = BOK_CALL_SELF;
        } else {
            callOp.kind = objindex_expr ? BOK_CALL_OTHER_IDX : BOK_CALL_OTHER;
            callOp.attr.call.objID = callobjid;
        }
        callOp.attr.call.funID = funid;
        callOp.attr.call.numResults = BCGetNumResults(func);
        if (callOp.attr.call.numResults >= 1) {
            extraResults = callOp.attr.call.numResults - 1;
        }
    } else {
    indirect_call: {
        // not a direct call: so instead we need to do a __call_methodptr(p) where p is the pointer
        AST *ident = node->left;
        AST *funcType = ExprType(ident);
        Function *func;
        int funid;
        if (!IsFunctionType(funcType)) {
            ERROR(node,"Unhandled indirect FUNCALL symbol");
            return;
        }
        indirect_call_expr = ident;
        // strip off one level of indirection if this is not a pointer
        if (!IsPointerType(indirect_call_expr) && indirect_call_expr->kind == AST_ARRAYREF && IsConstZero(indirect_call_expr->right)) {
            indirect_call_expr = indirect_call_expr->left;
            if (indirect_call_expr->kind == AST_MEMREF) {
                indirect_call_expr = indirect_call_expr->right;
            }
        }
        sym = LookupSymbol("__call_methodptr");
        if (!sym || sym->kind != SYM_FUNCTION) {
            ERROR(node,"Internal error, missing __call_methodptr");
            return;
        }
        func = (Function *)sym->val;
        funid = getFuncID(func->module, sym->our_name);
        if (funid < 0) {
            ERROR(node, "Unable to get function ID for %s", sym->our_name);
        }
        if (func->module == current) {
            callOp.kind = BOK_CALL_SELF;
        } else {
            callobjid = getObjID(current, func->module->classname, &objtype);
            callOp.kind = BOK_CALL_OTHER;
            callOp.attr.call.objID = callobjid;
        }
        callOp.attr.call.funID = funid;
        callOp.attr.call.numResults = BCGetNumResultsByType(funcType);
        if (callOp.attr.call.numResults >= 1) {
            extraResults = callOp.attr.call.numResults - 1;
        }
        }
    }

    if (anchorOp.kind) BIRB_PushCopy(irbuf,&anchorOp);

    if (node->right) {
        ASSERT_AST_KIND(node->right,AST_EXPRLIST,return;);
        for (AST *list=node->right;list;list=list->right) {
            ASSERT_AST_KIND(list,AST_EXPRLIST,return;);
            BCCompileExpression(irbuf,list->left,context,false);
            if (dupFirstParameter) {
                ByteOpIR regOp = { .kind = BOK_REG_WRITE, .data.int32 = HWRegRetval(1) };
                BIRB_PushCopy(irbuf,&regOp);
                regOp.kind = BOK_REG_READ;
                BIRB_PushCopy(irbuf,&regOp);
                BIRB_PushCopy(irbuf,&regOp);
            }
        }
    }

    if (vbase_expr) {
        ByteOpIR regOp = { .kind = BOK_REG_WRITE, .data.int32 = HWRegVBase()};
        BCCompileExpression(irbuf,vbase_expr,context,false);
        BIRB_PushCopy(irbuf,&regOp);
    }

    if (indirect_call_expr) {
        BCCompileExpression(irbuf,indirect_call_expr,context,false); // push as parameter
        ByteOpIR regPushOp = { .kind = BOK_REG_WRITE, .data.int32 = HWRegRetval(1) };
        BIRB_PushCopy(irbuf,&regPushOp);
    }
    if (objindex_expr) BCCompileExpression(irbuf,objindex_expr,context,false);
    BIRB_PushCopy(irbuf,&callOp);

    if (!interp_can_multireturn() && asExpression && extraResults) {
        // push extra result parameters
        ByteOpIR memOp = { .kind = BOK_REG_READ };
        memOp.attr.memop.memSize = MEMOP_SIZE_LONG;
        if (extraResults >= 1) {
            memOp.data.int32 = HWRegRetval(1);
            BIRB_PushCopy(irbuf, &memOp);
        }
        if (extraResults >= 2) {
            memOp.data.int32 = HWRegRetval(2);
            BIRB_PushCopy(irbuf, &memOp);
        }
        if (extraResults >= 3) {
            memOp.data.int32 = HWRegRetval(3);
            BIRB_PushCopy(irbuf, &memOp);
        }
        if (extraResults >= 4) {
            ERROR(node, "This interpreter cannot handle more than 4 return values from a function");
        }
    }

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

static int
NonUnsignedOp(int val)
{
    switch(val) {
    case K_LEU: return K_LE;
    case K_GEU: return K_GE;
    case K_LTU: return '<';
    case K_GTU: return '>';
    case K_LIMITMIN_UNS: return K_LIMITMIN;
    case K_LIMITMAX_UNS: return K_LIMITMAX;
    default: return val;
    }
}

static void
BCCompileCast(BCIRBuffer *irbuf,AST *node,BCContext context,bool asStatement)
{
    AST *newType = node->left;
    AST *expr = node->right;
    BCCompileExpression(irbuf,expr,context,asStatement);
    if (asStatement || !newType) return;
    // sanity check results
    AST *oldType = ExprType(expr);
    int newSize, oldSize;
    newSize = TypeSize(newType);
    oldSize = TypeSize(oldType);
    if (newSize != oldSize) {
        ERROR(node, "casts that change size not supported in bytecode yet");
        return;
    }
    if ( (IsFloatType(oldType) && !IsFloatType(newType) && !IsGenericType(newType)) || 
         (IsFloatType(newType) && !IsFloatType(oldType) && !IsGenericType(oldType)) ) {
        ERROR(node, "casts to/from float not supported in bytecode yet");
        return;
    }
}

static void
BCCompileExpression(BCIRBuffer *irbuf,AST *node,BCContext context,bool asStatement) {
    if(!node) {
        ERROR(NULL,"Internal Error: Null expression!!");
        return;
    }
    int siz = TypeSize(ExprType(node));
    int numLongs = (siz+LONG_SIZE-1)/LONG_SIZE;
    bool needsAbs = false;
    if (IsConstExpr(node)) {
        if (!asStatement) {
            // check for 64 bit values here
            int64_t val = EvalConstExpr(node);
            if (siz == 8) {
                BCCompileInteger(irbuf,val);
                BCCompileInteger(irbuf,val>>32);
            } else {
                BCCompileInteger(irbuf,val);
            }
        }
    } else {
        unsigned popResults = asStatement ? numLongs : 0;
        switch(node->kind) {
            case AST_EXPRLIST: {
                AST *list = node;
                while (list) {
                    node = list->left;
                    list = list->right;
                    // the default RETURN statement lists can have DECLARE_VAR in them
                    if (node->kind == AST_DECLARE_VAR) {
                        node = node->right;
                        if (!IsIdentifier(node)) {
                            ERROR(node,"bad return list");
                        }
                    }
                    BCCompileExpression(irbuf,node,context,false);
                }
                popResults = 0;
            } break;
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

                case K_LIMITMAX_UNS:
                case K_LIMITMIN_UNS: {
                    // TODO: don't do this nonsense when there's a native op for this
                    BCCompileExpression(irbuf,AstOperator('+',
                        AstOperator(NonUnsignedOp(optoken),
                            AstOperator('+',left,AstInteger(1U<<31)),
                            AstOperator('+',right,AstInteger(1U<<31))
                    ),AstInteger(1U<<31)),context,asStatement);

                    return;
                } break;
                case K_GEU:
                case K_GTU:
                case K_LEU:
                case K_LTU: {
                    BCCompileExpression(irbuf,AstOperator(NonUnsignedOp(optoken),
                            AstOperator('+',left,AstInteger(1U<<31)),
                            AstOperator('+',right,AstInteger(1U<<31))
                    ),context,asStatement);

                    return;
                }


                case K_SIGNEXTEND:
                case K_ZEROEXTEND: {
                    ByteOpIR mathOp = {.kind = BOK_MATHOP};
                    if (ExprHasSideEffects(right)) {
                        ERROR(node, "Bytecode output cannot handle side effects in right argument of sign/zero extend");
                        right = AstInteger(16);
                    }
                    BCCompileExpression(irbuf, left, context, false);
                    right = FoldIfConst(AstOperator('-', AstInteger(32), right));
                    BCCompileExpression(irbuf, right, context, false);
                    mathOp.mathKind = MOK_SHL;
                    BIRB_PushCopy(irbuf, &mathOp);
                    mathOp.mathKind = (optoken == K_SIGNEXTEND) ? MOK_SAR : MOK_SHR;
                    BCCompileExpression(irbuf, right, context, false);
                    BIRB_PushCopy(irbuf, &mathOp);
                    return;
                }
                case K_INCREMENT: 
                case K_DECREMENT:
                case '?':
                    goto modifyOp;
                default:
                    mok = Optoken2MathOpKind(optoken,&unary,&needsAbs);
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
                if (needsAbs) {
                    mathOp.kind = BOK_MATHOP;
                    mathOp.mathKind = MOK_ABS;
                    BIRB_PushCopy(irbuf,&mathOp);
                }
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
                if (node->right) {
                    // right side describes the type
                    // is it OK to ignore this??
                    //WARNING(node->right,"right side of AST_ADDROF not empty???");
                }
                BCCompileMemOp(irbuf,node->left,context,MEMOP_ADDRESS);
            } break;
            case AST_LOCAL_IDENTIFIER:
            case AST_IDENTIFIER: 
            case AST_SYMBOL: {
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
                case SYM_LABEL:
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
                case SYM_ALIAS:
                    // this is an alias for an expression, compile it
                {
                    AST *expr = (AST *)sym->val;
                    BCCompileExpression(irbuf, expr, context, asStatement);
                    return;
                }
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
            case AST_CAST: {
                BCCompileCast(irbuf, node, context, asStatement);
            } break;
            case AST_METHODREF: {
                BCCompileMemOp(irbuf,node,context,MEMOP_READ);
            } break;
            case AST_MEMREF: {
                // some frontends neglect to dereference this
                AST *newNode = NewAST(AST_ARRAYREF, node, AstInteger(0));
                BCCompileMemOp(irbuf,newNode,context,MEMOP_READ);
            } break;
            case AST_SELF: {
                AST *newNode = AstIdentifier("__interp_vbase");
                BCCompileMemOp(irbuf,newNode,context,MEMOP_READ);
            } break;
            case AST_ALLOCA: {
                // compile as "OUTB = __interp_dcurr; __interp_dcurr += ((n+3)&~3); OUTB"
                AST *expr = BCAllocaExpr(node->right);
                BCCompileExpression(irbuf, expr, context, asStatement);
            } break;
            case AST_GETLOW: {
                AST *expr = node->left;
                if (IsIdentifier(expr)) {
                    expr = NewAST(AST_MEMREF,
                                  ast_type_long,
                                  NewAST(AST_ADDROF, expr, NULL));
                    BCCompileExpression(irbuf, expr, context, asStatement);
                } else {
                    BCCompileExpression(irbuf, expr, context, asStatement);
                    // pop the high word off the stack
                    BCCompilePopN(irbuf, 1);
                }
            } break;
            case AST_GETHIGH: {
                AST *expr = node->left;
                expr = NewAST(AST_MEMREF,
                              ast_type_long,
                              AstOperator('+',
                                          NewAST(AST_ADDROF, expr, NULL),
                                          AstInteger(4) )
                                  );
                BCCompileExpression(irbuf, expr, context, asStatement);
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
    case AST_LABEL: {
        const char *name = GetUserIdentifierName(node->left);
        if (!name) ERROR(node,"Can't get name of label");
        ByteOpIR label = {.kind = BOK_NAMEDLABEL,.attr.labelHiddenVars = context.hiddenVariables,.data.stringPtr = name};
        BIRB_PushCopy(irbuf,&label);
        if (node->right) {
            ASSERT_AST_KIND(node->right,AST_STMTLIST,break;);
            BCCompileStmtlist(irbuf,node->right,context);
        }
    } break;
    case AST_GOTO: {
        const char *name = GetIdentifierName(node->left);
        if (!name) ERROR(node,"Can't get name to goto");
        BCCompileJump(irbuf,BCNewNamedLabelRef(context,name),context);
    } break;
    case AST_GOSUB: {
        AST *identAddr = NewAST(AST_ABSADDROF,node->left,NULL);
        AST *gosubCall = AstIdentifier("__gosub_helper");
        AST *exprList = NULL;

        exprList = NewAST(AST_EXPRLIST, AstIdentifier("__interp_pbase"), exprList);
        exprList = NewAST(AST_EXPRLIST, AstIdentifier("__interp_vbase"), exprList);
        exprList = NewAST(AST_EXPRLIST, identAddr, exprList);
        gosubCall = NewAST(AST_FUNCCALL, gosubCall, exprList);
        BCCompileFunCall(irbuf,gosubCall,context,false,false);
    } break;
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

        if (fromExpression == NULL && IsConstExpr(stepExpression) && llabs(EvalConstExpr(stepExpression))==1) {
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
            newcontext.countRepeatAt = newcontext.hiddenVariables;
            ByteOpIR *topLabel = BCNewOrphanLabel(newcontext);
            ByteOpIR *nextLabel = BCNewOrphanLabel(newcontext);
            ByteOpIR *quitLabel = BCNewOrphanLabel(context);
            newcontext.nextLabel = nextLabel;
            newcontext.quitLabel = quitLabel;

            if (!isConst || constVal == 0) BCCompileJumpEx(irbuf,quitLabel,BOK_JUMP_TJZ,newcontext,-1,false); 
            BIRB_Push(irbuf,topLabel);

            if (body) {
                ASSERT_AST_KIND(body,AST_STMTLIST,return;);
                BCCompileStmtlist(irbuf,body,newcontext);
            } else DEBUG(node,"Compiling empty REPEAT N loop?");

            BIRB_Push(irbuf,nextLabel);
            BCCompileJumpEx(irbuf,topLabel,BOK_JUMP_DJNZ,newcontext,0,false);
            BIRB_Push(irbuf,quitLabel);
        } else {
            // REPEAT FROM TO
            bool stepConst = IsConstExpr(stepExpression);
            int32_t stepConstVal = stepConst ? EvalConstExpr(stepExpression) : 0;
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
            if (haveStep) {
                if ( 0 > (int32_t)stepConstVal ) {
                    BCCompileInteger(irbuf, -stepConstVal);
                } else {
                    BCCompileExpression(irbuf,stepExpression,newcontext,false);
                }
            }
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
        ByteOpIR pushEnd = {.kind=BOK_FUNDATA_PUSHADDRESS,.jumpTo=endlabel,.attr.pushaddress.forJump=true};
        BIRB_PushCopy(irbuf,&pushEnd);
        // Compile switch expression
        BCCompileExpression(irbuf,node->left,context,false);

        ASSERT_AST_KIND(node->right,AST_STMTLIST,return;)

        BCContext newcontext = context;
        newcontext.hiddenVariables += 2;
        newcontext.caseVarsAt = newcontext.hiddenVariables;

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
                if (item->right && (item->right->kind == AST_CASEITEM || item->right->kind == AST_OTHER)) {
                    // consolidate series of cases
                    AST *origitem = item;
                    AST *list = (item->right->kind == AST_CASEITEM) ? item->left : NULL;
                    bool isDefault = false;
                    AST *stmt = NULL;
                    while (item->right) {
                        if (item->right->kind == AST_CASEITEM) {
                            list = AppendList(list, item->right->left);
                        } else if (item->right->kind == AST_OTHER) {
                            isDefault = true;
                        } else {
                            stmt = item->right;
                            break;
                        }
                        item = item->right;
                    }
                    if (isDefault) {
                        WARNING(origitem, "Potentially dodgy case statement compilation in bytecode\n");
                        origitem->kind = AST_OTHER;
                        origitem->left = stmt;
                        origitem->right = NULL;
                        if (othercase) ERROR(item,"Multiple OTHER cases");
                        othercase = origitem;
                    } else {
                        origitem->kind = AST_CASEITEM;
                        origitem->left = list;
                        origitem->right = item->right;
                    }
                }
            } else if (item->kind == AST_OTHER) {
                //printf("Got AST_OTHER\n");
                if (othercase) ERROR(item,"Multiple OTHER cases");
                othercase = item;
                AST *stmt = othercase->left;
                // ignore cases if there's a default
                while (stmt && stmt->kind == AST_CASEITEM) {
                    stmt = stmt->right;
                }
                othercase->left = stmt;
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

            if (item->kind == AST_CASEITEM || item->kind == AST_OTHER) {
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
    case AST_JUMPTABLE: {
        ASSERT_AST_KIND(node->left,AST_ASSIGN,return;);
        ASSERT_AST_KIND(node->left->left,AST_CASEEXPR,return;);
        
        ByteOpIR align = {.kind = BOK_ALIGN,.data.int32 = 2};
        if (!gl_p2) BIRB_PushCopy(irbuf->pending,&align); // P2 doesn't need alignment
        ByteOpIR *tableLabel = BCNewOrphanLabel(context);
        BIRB_Push(irbuf->pending,tableLabel);

        ByteOpIR *endLabel = BCNewOrphanLabel(context);

        BCContext newcontext = context;
        newcontext.hiddenVariables += 2;
        newcontext.caseVarsAt = newcontext.hiddenVariables;

        ByteOpIR pushDoneAddr = {.kind = BOK_FUNDATA_PUSHADDRESS,.jumpTo=endLabel,.attr.pushaddress.forJump=true};
        BIRB_PushCopy(irbuf,&pushDoneAddr);
        BCCompileInteger(irbuf,0); // Fake index var so we can use CASE_DONE

        BCCompileExpression(irbuf,node->left->right,context,false);
        ByteOpIR lookupOp = {.kind = BOK_FUNDATA_LOOKUPJUMP,.jumpTo=tableLabel};
        BIRB_PushCopy(irbuf,&lookupOp);
        // There's no straightup "jump to value+PBASE". CASE_DONE does it, but we need to push a dummy value first
        BCCompileInteger(irbuf,0);
        ByteOpIR jumpOp = {.kind = BOK_CASE_DONE};
        BIRB_PushCopy(irbuf,&jumpOp);

        AST *ast = node->right;
        for (;ast&&ast->kind==AST_LISTHOLDER;ast=ast->right) {
            const char *name = GetUserIdentifierName(ast->left);
            if (!name) ERROR(ast,"Can't get name of label");
            ByteOpIR jumpEntry = {.kind = BOK_FUNDATA_JUMPENTRY,.jumpTo = BCNewNamedLabelRef(newcontext,name)};
            BIRB_PushCopy(irbuf->pending,&jumpEntry);
        }
        if (ast) BCCompileStmtlist(irbuf,ast,newcontext);
        BIRB_Push(irbuf,endLabel);
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
    do_quitloop:
    case AST_QUITLOOP: {
        if (!context.nextLabel) {
            ERROR(node,"QUIT outside loop");
        } else {
            if (context.hiddenVariables == context.countRepeatAt && context.hiddenVariables-1 == context.quitLabel->attr.labelHiddenVars && (curfunc->optimize_flags & OPT_PEEPHOLE)) {
                // Since the count variable in a counted repeat can never be zero, instead of compiling a pop+jump, we can use a jump_if_nz
                BCCompileJumpEx(irbuf,context.quitLabel,BOK_JUMP_IF_NZ,context,-1,true);
            } else {
                BCCompileJump(irbuf,context.quitLabel,context);
            }
        }
    } break;
    case AST_ENDCASE: {
        if (context.caseVarsAt < 0) {
            if (IsCLang(curfunc->language) && context.nextLabel) {
                // in C "break" acts as both ENDCASE and QUIT
                goto do_quitloop;
            }
            ERROR(node,"ENDCASE outside of a CASE");
        }
        BCCompilePopN(irbuf,context.hiddenVariables - context.caseVarsAt);
        ByteOpIR doneOp = {.kind = BOK_CASE_DONE};
        BIRB_PushCopy(irbuf,&doneOp);
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
        // TODO: This generates explicit returns of the result var. Fix in peephole step?
        bool isAbort = node->kind == AST_THROW;
        AST *retval = node->left;
        ByteOpIR returnOp = {0};
        returnOp.attr.returninfo.numResults = BCGetNumResults(curfunc);

        if (retval && returnOp.attr.returninfo.numResults == 1 && AstMatch(curfunc->resultexpr, retval)) {
            retval = NULL; // return RESULT => return
        }
        returnOp.kind = isAbort ? (retval ? BOK_ABORT_POP : BOK_ABORT_PLAIN) : (retval ? BOK_RETURN_POP : BOK_RETURN_PLAIN);
        if (retval) {
            if (retval->kind == AST_DECLARE_VAR) { // handle declared types in return values
                retval = retval->right;
            }
            if (returnOp.attr.returninfo.numResults > 1 && IsIdentifier(retval)) {
                // return x when x is large type needs multiple pushes
                int size = TypeSize(ExprType(retval)) / LONG_SIZE;
                ASTReportInfo save;
                AST *elem, *list = NULL;
                if (size != returnOp.attr.returninfo.numResults) {
                    ERROR(node,"Return value size does not match function return size");
                    size = returnOp.attr.returninfo.numResults;
                }
                AstReportAs(node,&save);
                AST *ref = NewAST(AST_ADDROF, retval, NULL);
                ref = NewAST(AST_MEMREF, ast_type_long, ref);
                for (int i = 0; i < size; i++) {
                    elem = NewAST(AST_ARRAYREF, ref, AstInteger(i));
                    list = AddToList(list, NewAST(AST_EXPRLIST, elem, NULL));
                }
                AstReportDone(&save);
                retval = list;
            }
            BCCompileExpression(irbuf,retval,context,false);
        }
        if (returnOp.attr.returninfo.numResults > 1 && !interp_can_multireturn()) {
            if (returnOp.kind != BOK_RETURN_POP) {
                ERROR(node, "Internal issue, cannot handle multi-assign for this return/abort");
            }
            // pop extra values off stack into temporary registers
            ByteOpIR memOp = { .kind = BOK_REG_WRITE };
            int n;
            memOp.attr.memop.memSize = MEMOP_SIZE_LONG;
            n = returnOp.attr.returninfo.numResults - 1;
            while (n > 0) {
                memOp.data.int32 = HWRegRetval(n);
                BIRB_PushCopy(irbuf,&memOp);
                --n;
            }
        }
        BIRB_PushCopy(irbuf,&returnOp);
    } break;
    case AST_YIELD: {
        // do nothing  
    } break;
    case AST_STMTLIST: {
        BCCompileStmtlist(irbuf,node,context);
    } break;
    case AST_SCOPE: {
        // TODO: When we implement __builtin_alloca(), we should restore the
        // stack after compiling the contents of the scope.
        BCCompileStmtlist(irbuf,node->left,context);
    } break;
    case AST_INLINEASM: {
        ERROR(node, "Inline assembly is not supported in bytecode output");
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

    // make sure function offsets are good
    NormalizeVarOffsets(F);
    
    // first up, we now know the function's address, so let's fix it up in the header
    int func_ptr = bob->total_size;
    int func_offset = 0;
    int func_localsize = BCLocalSize(F); // F->numlocals*4; // SUPER DUPER FIXME smaller locals
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
    } else if (F->body->kind == AST_STRING) {
        DEBUG(NULL,"compiling function %s which is just a reference",F->name);
    } else if (F->body->kind != AST_STMTLIST) {
        ERROR(F->body,"Internal Error: Expected AST_STMTLIST, got id %d",F->body->kind);
        return;
    } else {
        BCContext context = {.caseVarsAt = -1,.countRepeatAt = -1};
        if (F->closure) {
            // actually need variables to be in the heap
            // so insert some code to copy them
            AST *allocmem;
            AST *copymem;
            AST *args;
            AST *nodeInfo = F->body;
            int framesize = FuncLocalSize(F);
            ASTReportInfo save;

            WARNING(nodeInfo, "Closures are not fully supported in bytecode yet");
            AstReportAs(nodeInfo, &save);
            AST *ident_temp = NewAST(AST_RESULT, NULL, NULL);
            
            // result = _gc_alloc_managed(framesize)
            args = NewAST(AST_EXPRLIST, AstInteger(framesize), NULL);
            allocmem = NewAST(AST_FUNCCALL, AstIdentifier("_gc_alloc_managed"), args);
            allocmem = AstAssign(ident_temp, allocmem);
            allocmem = NewAST(AST_STMTLIST, allocmem, NULL);
            // longcopy(result, dbase, framesize)
            args = NewAST(AST_EXPRLIST, AstInteger(framesize), NULL);
            args = NewAST(AST_EXPRLIST, AstIdentifier("__interp_dbase"), args);
            args = NewAST(AST_EXPRLIST, ident_temp, args);
            copymem = NewAST(AST_FUNCCALL, AstIdentifier("bytemove"), args);
            copymem = NewAST(AST_STMTLIST, copymem, NULL);
            // dbase := result
            args = AstAssign(AstIdentifier("__interp_vbase"), ident_temp);
            args = NewAST(AST_STMTLIST, args, F->body);
            allocmem = AddToList(allocmem, copymem);
            allocmem = AddToList(allocmem, args);
            F->body = allocmem;
            AstReportDone(&save);
        }
    
        BCCompileStmtlist(&irbuf,F->body,context);
    }
    // Only need to append a return for void functions
    int numPushed = BCGetNumResults(F);
    if (numPushed <= 1) {
        ByteOpIR retop = {0,.kind = BOK_RETURN_PLAIN,.attr.returninfo.numResults=numPushed};
        BIRB_PushCopy(&irbuf,&retop);
    }

    BIRB_AppendPending(&irbuf);

    BCIR_ResolveNamedLabels(&irbuf);

    BCIR_Optimize(&irbuf);

    BCIR_to_BOB(&irbuf,bob,func_ptr-ModData(current)->compiledAddress);
    
    curfunc = NULL;
}

static void BCPrepareObject(Module *P);

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
BCAddStaticReference(Module *P, AST *obj) {
    ASSERT_AST_KIND(obj,AST_OBJECT,return;);
    for (int i=0;i<ModData(P)->obj_cnt;i++) {
        if (AstMatch(obj,ModData(P)->objs[i])) return;
    }
    if (ModData(P)->obj_cnt == BC_MAX_POINTERS) {
        ERROR(obj, "Too many objects in class"); return;
    }
    ModData(P)->objs[ModData(P)->obj_cnt++] = obj;
    BCPrepareObject(GetClassPtr(obj));
    DEBUG(obj,"Added static OBJ reference for %s",GetClassPtr(obj)->classname);
}

static void
BCAddPointerCalledObjects(Module *P, AST *ast) {

    if (!ast) return;

    switch (ast->kind) {
    case AST_ADDROF:
    case AST_FUNCCALL:
        if (ast->left->kind == AST_METHODREF) {
            AST *methodref = ast->left;
            AST *receiver = methodref->left;
            if (!receiver) return;
            if (receiver->kind == AST_ARRAYREF) receiver = receiver->left; // ???
            if (receiver->kind == AST_MEMREF) {
                BCAddStaticReference(P,receiver->left);
            } else if (IsIdentifier(receiver)) {
                Symbol *objsym = LookupAstSymbol(receiver,NULL);
                if (objsym) DEBUG(ast,"Got identifier call with symbol kind %d",objsym->kind);
                else DEBUG(ast,"Got identifier call with null symbol");
                if (objsym && objsym->kind != SYM_VARIABLE) {
                    DEBUG(ast,"Got non-variable call");
                    AST *type = ExprType(receiver);
                    if (type && type->kind == AST_OBJECT) BCAddStaticReference(P,type);
                }
            }
        }
        // Fall through to find nested function calls
    default:
        BCAddPointerCalledObjects(P,ast->left);
        BCAddPointerCalledObjects(P,ast->right);
        break;
    }
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

    // insert system module
    if (systemModule && systemModule != P && systemModule->functions && P->functions) {
        BCInsertModule(P, systemModule, "_system_");
    }

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

    
    {
        int obj_cnt = 0;
        // Count and gather object members
        for (AST *upper = P->finalvarblock; upper; upper = upper->right) {
            if (upper->kind != AST_LISTHOLDER) {
                ERROR(upper, "Internal error, expected listholder");
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
                DEBUG(NULL,"Got obj array of type %s, size %" PRId64 " named %s",((Module*)var->left->d.ptr)->classname,var->right->left->right->d.ival,var->right->left->left->d.string);
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

    // Register static references for objects that are called through pointers
    for (Function *f = P->functions; f; f = f->next) {
        if (ShouldSkipFunction(f)) continue;
        Function *fsave = curfunc;
        curfunc = f;
        BCAddPointerCalledObjects(P,f->body);
        curfunc = fsave;
    }

    current = save;
    
}

static void
BCEmitModuleRelocations(ByteOutputBuffer *bob,Module *P) {
    BCRelocList *list = ModData(P)->relocList;
    uint32_t addr = ModData(P)->compiledAddress;
    while (list) {
        if (list->func) {
            (*list->func)(P, list->pos, addr);
        } else {
            ERROR(NULL, "No relocation function provided");
        } 
        list = list->next;
    }
}

static void
BCDatPutLong(uint8_t *ptr, uint32_t data) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    data = __builtin_bswap32(data);
#endif
    memcpy(ptr, &data, 4);
}

static void
BCProcessDatRelocs(Module *P, OutputSpan *span, Reloc *reloc, size_t numRelocs)
{
    size_t i;
    int datbase = BCgetDAToffset(P, true, NULL, true);
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
                BCDatPutLong(&spdata[off], data);
            } else if (sym->kind == SYM_FUNCTION) {
                Function *F = (Function *)sym->val;
                Module *Obj = F->module;
                if (Obj->bedata == NULL) {
                    ERROR(NULL, "No module data for %s", Obj->classname);
                    return;
                }
                int32_t addr = ModData(Obj)->compiledAddress;
                int id = getFuncIDForKnownFunc(Obj, F);
                if (id < 0) {
                    ERROR(NULL, "Unable to get id for function %s", F->name);
                }
                if (addr < 0) {
                    BCCompileDatModuleFuncRef(&spdata[off], Obj, id);
                } else {
                    data = (id<<16) | addr;
                    BCDatPutLong(&spdata[off], data);
                }
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


static void
BCCompileObject(ByteOutputBuffer *bob, Module *P) {

    Module *save = current;
    current = P;

    DEBUG(NULL,"Compiling bytecode for object %s",P->fullname);

    BOB_Align(bob,4); // Long-align

    BOB_Comment(bob,auto_printf(128,"--- Object Header for %s",P->classname));

    // prepare object
    BCPrepareObject(P);

    if (ModData(P)->compiledAddress < 0) {
        ModData(P)->compiledAddress = bob->total_size;
        BCEmitModuleRelocations(bob,P);
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
            BOB_PushWord(bob,varOffset,obj->kind == AST_OBJECT?"(Static Reference)":"VAR offset"); // VAR Offset (from current object's VAR base)
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
        Flexbuf datRelocs;
        OutputSpan *datSpan;
        flexbuf_init(&datBuf,2048);
        flexbuf_init(&datRelocs,1024);
        // FIXME for some reason, this sometimes(??) prints empty DAT blocks for subobjects ???
        PrintDataBlock(&datBuf,P->datblock,NULL,&datRelocs);
        BOB_Comment(bob,"--- DAT Block");
        datSpan = BOB_Push(bob,(uint8_t*)flexbuf_peek(&datBuf),flexbuf_curlen(&datBuf),NULL);
        BCProcessDatRelocs(P,datSpan,(Reloc*)flexbuf_peek(&datRelocs),flexbuf_curlen(&datRelocs)/sizeof(Reloc));
        flexbuf_delete(&datRelocs);
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

    if (obj_cnt != ModData(P)->obj_cnt) {
        ERROR(NULL,"Object count for %s changed!", P->classname);
    }
    // emit subobjects
    for (int i=0;i<obj_cnt;i++) {
        AST *obj = ModData(P)->objs[i];
        if (obj->kind == AST_DECLARE_VAR) obj = obj->left;
        Module *mod = GetClassPtr(obj);
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

static void BCAddHeap(ByteOutputBuffer *bob, Module*P) {
    Symbol *sym = LookupSymbolInTable(&systemModule->objsyms, "__heap_base");
    Symbol *objsym = LookupSymbolInTable(&P->objsyms, "_system_");
    Label *L;

    if (! (gl_features_used & FEATURE_NEED_HEAP)) return;
    if (!objsym) return;
    if (!sym || sym->kind != SYM_LABEL) return;
    
    L = (Label *)sym->val;
    int off = L->hubval;
    off += BCgetDAToffset(systemModule,true,NULL,false);

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

    // Align and append any needed heap
    BOB_Align(&bob,4);
    BCAddHeap(&bob,P);

    const int programSize = bob.total_size - headerSize;
    const int variableSize = P->varsize; // Already rounded up!
    const int stackBase = headerSize + programSize + variableSize + 8; // space for that stack init thing

    if (gl_printprogress) {
        printf("Program size:  %6d bytes\n",programSize);
        printf("Variable size: %6d bytes\n",variableSize);
        if (!gl_p2) printf("Stack/Free:    %6d bytes\n",0x8000-(headerSize+programSize+variableSize));
        else printf("Stack/Free:    %6d TODO\n",0);
        //printf("Stack base = %x\n", stackBase);
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
