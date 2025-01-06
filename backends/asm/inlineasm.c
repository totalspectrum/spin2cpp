//
// Inline assembly code
//
// Copyright 2016-2025 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include <stdlib.h>
#include "spinc.h"
#include "outasm.h"
#include "backends/becommon.h"

Operand *
GetLabelOperand(const char *name, bool inFcache)
{
    Operand *op;

    name = NewTempLabelName();
    if (inFcache) {
        op = NewOperand(IMM_COG_LABEL, name, 0);
    } else if (curfunc && curfunc->code_placement == CODE_PLACE_HUB) {
        op = NewOperand(IMM_HUB_LABEL, name, 0);
    } else {
        op = NewOperand(IMM_COG_LABEL, name, 0);
    }
    return op;
}

Operand *
GetLabelFromSymbol(AST *where, const char *name, bool inFcache)
{
    Symbol *sym;
    sym = FindSymbol(&curfunc->localsyms, name);
    if (!sym || sym->kind != SYM_LOCALLABEL) {
        ERROR(where, "%s is not a label in this function", name);
        return NULL;
    }
    if (!sym->v.ptr) {
        sym->v.ptr = (void *)GetLabelOperand(name, inFcache);
    }
    return (Operand *)sym->v.ptr;
}

extern void ValidateStackptr(void);
extern Operand *stackptr;
extern void ValidateFrameptr(void);
extern Operand *frameptr;
extern void ValidateObjbase(void);
extern Operand *objbase;
extern void ValidateHeapptr(void);
extern Operand *heapptr;
extern Operand *resultreg[];
extern Operand *arg1, *arg2;

static int isargdigit(int c)
{
    if (c >= '0' && c <= '9') {
        return 1;
    } else {
        return 0;
    }
}

static int parseargnum(const char *n)
{
    int reg = 0;
    reg = *n++ - '0';
    if (*n) {
        reg = 10*reg + (*n - '0');
    }
    if (reg < 1 || reg > 99) {
        ERROR(NULL, "internal error; unexpected arg/result number");
        return 0;
    }
    return reg-1;
}

extern Operand *LabelRef(IRList *irl, Symbol *sym);

Operand *ImmediateRef(int immflag, intptr_t val)
{
    char buf[100];
    if (immflag) {
        return NewImmediate(val);
    } else {
        sprintf(buf, "%lu", (unsigned long)val);
        return NewOperand(REG_HW, strdup(buf), 0);
    }
}

//
// compile an expression as an inline asm operand
//
static Operand *
CompileInlineOperand(IRList *irl, AST *expr, int *effects, int immflag)
{
    Operand *r = NULL;
    int r_address = 0;
    int32_t v;

    if (expr->kind == AST_IMMHOLDER || expr->kind == AST_BIGIMMHOLDER) {
        immflag = 1;
        expr = expr->left;
    }
    // labels get automatically converted to array references; undo that
    if (expr->kind == AST_ARRAYREF && IsConstExpr(expr->right) && EvalConstExpr(expr->right) == 0)
    {
        expr = expr->left;
    }

    if (expr->kind == AST_LOCAL_IDENTIFIER || expr->kind == AST_IDENTIFIER || expr->kind == AST_RESULT) {
        Symbol *sym;
        const char *name;
        if (expr->kind == AST_LOCAL_IDENTIFIER) {
            expr = expr->left;
        }
        if (expr->kind == AST_RESULT) {
            name = "result";
        } else {
            name = expr->d.string;
        }
        sym = LookupSymbol(name);
        if (!sym) {
            // check for special symbols "objptr" and "sp"
            if (!strcmp(name, "objptr")) {
                ValidateObjbase();
                r = objbase;
                r_address = immflag;
            }
            else if (!strcmp(name, "sp")) {
                ValidateStackptr();
                r = stackptr;
                r_address = immflag;
            }
            else if (!strcmp(name, "fp")) {
                ValidateFrameptr();
                r = frameptr;
                r_address = immflag;
            }
            else if (!strcmp(name, "__heap_ptr")) {
                ValidateHeapptr();
                r = heapptr;
                r_address = immflag;
            }
            else if (!strncmp(name, "result", 6) && isargdigit(name[6]) && !name[7]) {
                r = GetResultReg( parseargnum(name+6));
                r_address = immflag;
            }
            else if (!strncmp(name, "arg", 3) && isargdigit(name[3]) && isargdigit(name[4]) && !name[5])
            {
                r = GetArgReg( parseargnum(name+3) );
                r_address = immflag;
            } else if (!strncmp(name, "builtin_", 8)) {
                r = NewOperand(IMM_COG_LABEL, name, 0);
            } else {
                ERROR(expr, "Undefined symbol %s", name);
                return NewImmediate(0);
            }
        }
        if (!r) {
            switch(sym->kind) {
            case SYM_PARAMETER:
            case SYM_RESULT:
            case SYM_LOCALVAR:
            case SYM_TEMPVAR:
            case SYM_VARIABLE:
                r = CompileIdentifier(irl, expr);
                if (!r) {
                    ERROR(expr, "Bad identifier expression %s", sym->user_name);
                    return NewImmediate(0);
                }
                r_address = immflag;
                if (r->kind == HUBMEM_REF) {
                    if (sym->kind == SYM_VARIABLE) {
                        ERROR(expr, "Variable %s is placed in hub memory and hence cannot be accessed in inline assembly", sym->user_name);
                    } else {
                        ERROR(expr, "Variable %s must be placed in memory (probably due to an @ expression) and hence cannot be accessed in inline assembly", sym->user_name);
                    }
                }
                break;
            case SYM_CONSTANT:
                v = EvalPasmExpr(expr);

                /*  if (!immflag) {
                    WARNING(expr, "symbol %s used without #", sym->user_name);
                    } */
                r = ImmediateRef(immflag, v);
                break;
            case SYM_LOCALLABEL:
                r = GetLabelFromSymbol(expr, sym->our_name, false);
                immflag = 0;
                break;
            case SYM_LABEL:
                if (!immflag) {
                    ERROR(expr, "Variable %s is a hub label", sym->user_name);
                }
                r = LabelRef(irl, sym);
                break;
            case SYM_HWREG:
            {
                HwReg *hw = (HwReg *)sym->v.ptr;
                r = GetOneGlobal(REG_HW, hw->name, 0);
                r_address = immflag;
                break;
            }
            case SYM_FUNCTION:
            {
                if (curfunc && !strcmp(curfunc->name, sym->our_name) && IsBasicLang(curfunc->language)) {
                    // BASIC lets you write the function name to indicate the
                    // function result; allow that in inline asm too
                    // this is just like result1
                    r = GetResultReg(0);
                    r_address = immflag;
                    break;
                }
                /* otherwise fall through */
            }
            default:
                ERROR(expr, "Symbol %s is not usable in inline asm", sym->user_name);
                return NULL;
            }
        }
        if (r_address) {
            WARNING(expr, "Using # on registers in inline assembly may confuse the optimizer");
            return GetLea(irl, r);
        }
        return r;
    } else if (expr->kind == AST_INTEGER) {
        return ImmediateRef(immflag, expr->d.ival);
    } else if (expr->kind == AST_ADDROF) {
        r = CompileInlineOperand(irl, expr->left, effects, immflag);
        if (r && effects) {
            *effects |= OPEFFECT_FORCEHUB;
        }
        return r;
    } else if (expr->kind == AST_HWREG) {
        HwReg *hw = (HwReg *)expr->d.ptr;
        r = GetOneGlobal(REG_HW, hw->name, 0);
        if (immflag) {
            r = GetLea(irl, r);
        }
        return r;
    } else if (expr->kind == AST_CATCH) {
        r = CompileInlineOperand(irl, expr->left, effects, 0);
        if (r && effects) {
            *effects |= OPEFFECT_FORCEABS;
        }
        return r;
    } else if (expr->kind == AST_HERE) {
        /* handle $ */
        return NewPcRelative(0);
    } else if (IsConstExpr(expr)) {
        int32_t val = EvalConstExpr(expr);
        if (val == 0 && !immflag && expr->kind == AST_OPERATOR && expr->d.ival == '-' && expr->left && expr->right && expr->right->kind == AST_INTEGER && expr->right->d.ival == 0) {
            *effects |= OPEFFECT_DUMMY_ZERO;
        }
        return ImmediateRef(immflag, val);
    } else if (expr->kind == AST_RANGEREF && expr->left && expr->left->kind == AST_HWREG) {
        // something like ptrb[4]
        AST *rhs = expr->right;
        HwReg *hw = (HwReg *)expr->left->d.ptr;
        int32_t offset;
        if (!rhs || rhs->kind != AST_RANGE || rhs->right) {
            ERROR(rhs, "bad ptra/ptrb expression");
            offset = 0;
        } else {
            AST *val = rhs->left;
            if (val && (val->kind == AST_BIGIMMHOLDER || val->kind == AST_IMMHOLDER)) {
                val = val->left;
            }
            if (!IsConstExpr(val)) {
                ERROR(rhs, "ptra/ptrb offset must be constant");
                offset = 0;
            } else {
                offset = EvalConstExpr(val);
            }
        }
        r = GetOneGlobal(REG_HW, hw->name, 0);
        *effects |= (offset << OPEFFECT_OFFSET_SHIFT);
        return r;
    } else if (expr->kind == AST_OPERATOR) {
        // have to handle things like ptra++
        if (expr->d.ival == K_INCREMENT || expr->d.ival == K_DECREMENT) {
            int incdec = 0;
            AST *subexpr = NULL;
            if (expr->left && expr->left->kind == AST_HWREG) {
                incdec = (expr->d.ival == K_INCREMENT) ? OPEFFECT_POSTINC : OPEFFECT_POSTDEC;
                subexpr = expr->left;
            } else if (expr->right && expr->right->kind == AST_HWREG) {
                incdec = (expr->d.ival == K_INCREMENT) ? OPEFFECT_PREINC : OPEFFECT_PREDEC;
                subexpr = expr->right;
            }
            if (incdec && subexpr) {
                r = CompileInlineOperand(irl, subexpr, effects, 0);
                if (r && effects) {
                    *effects |= incdec;
                }
                return r;
            }
        }
        if (IsConstExpr(expr)) {
            int x = EvalPasmExpr(expr);
            return ImmediateRef(immflag, x);
        }
        /* handle $+x / $-x */
        if (expr->d.ival == '+' || expr->d.ival == '-') {
            int sign = expr->d.ival == '-' ? -1 : +1;

            // move constant part to rhs
            if (sign > 0 && IsConstExpr(expr->left)) {
                AST *tmp = expr->left;
                expr->left = expr->right;
                expr->right = tmp;
            }
            if (expr->left && expr->left->kind == AST_HERE) {
                if (expr->right && IsConstExpr(expr->right)) {
                    v = sign * EvalPasmExpr(expr->right);
                    return NewPcRelative(v);
                }
            }
            // handle a+n where a is an array
            if (expr->left && ( (expr->left->kind == AST_ARRAYREF && IsConstExpr(expr->left->right)) || IsIdentifier(expr->left) ) && IsConstExpr(expr->right) ) {
                int offset = (expr->left->kind == AST_ARRAYREF) ? EvalConstExpr(expr->left->right) : 0;
                AST *ref = (expr->left->kind == AST_ARRAYREF) ? expr->left->left : expr->left;
                //AST *typ = ExprType(ref);
                offset = offset + sign * EvalConstExpr(expr->right);
                r = CompileInlineOperand(irl, ref, effects, immflag);
                if (offset != 0) {
                    switch (r->kind) {
                    case IMM_COG_LABEL:
                    case IMM_HUB_LABEL:
                    case REG_HW:
                    case HUBMEM_REF:
                    case COGMEM_REF:
                        r->val += offset;
                        break;
                    default:
                        r = SubRegister(r, offset * LONG_SIZE);
                        break;
                    }
                }
                return r;
            }
        }
    }

    ERROR(expr, "Operand too complex for inline assembly");
    return NULL;
}

//
// compile an inline instruction
// ast points to an AST_INSTRUCTION, or the comments before it
//
#define MAX_OPERANDS 4

static IR *
CompileInlineInstr_only(IRList *irl, AST *ast, bool isInFcache)
{
    Instruction *instr;
    IR *ir;
    int numoperands;
    AST *operands[MAX_OPERANDS];
    uint32_t opimm[MAX_OPERANDS];
    int effects[MAX_OPERANDS];
    int i;
    uint32_t effectFlags = 0;
    uint32_t ival;
    uint32_t condbits;

    while (ast && ast->kind != AST_INSTR) {
        ast = ast->right;
    }
    if (!ast) {
        ERROR(NULL, "Internal error, expected instruction");
        return NULL;
    }
    instr = (Instruction *)ast->d.ptr;
    ir = NewIR(instr->opc);
    ir->instr = instr;
    ir->line = ast;
    
    /* parse operands and put them in place */
    ival = instr->binary;
    ival |= (gl_p2) ? (0xf<<28) : (0xf<<18); // set initial condition flags

    numoperands = DecodeAsmOperands(instr, ast, operands, opimm, &ival, &effectFlags);
    /* replace wcz with wc,wz if we can, to make the optimizer's job
       easier */
    if ( (effectFlags & FLAG_WCZ) && instr->flags != FLAG_P2_CZTEST ) {
        effectFlags &= ~FLAG_WCZ;
        effectFlags |= (FLAG_WZ|FLAG_WC);
    }
    ir->flags = effectFlags;
    ir->flags |= FLAG_USER_INSTR;
    if (isInFcache)
        ir->flags |= FLAG_USER_FCACHE;
    // check for conditional execution
    if (gl_p2) {
        condbits = ival >> 28;
    } else {
        condbits = (ival >> 18) & 0xf;
    }
    if (condbits==0 && gl_p2) {
        IR *newir = NewIR(OPC_RET);
        ir->next = newir;
        //ERROR(ast, "Cannot handle _ret_ on instruction in inline asm; convert to regular ret for flexspin compatibility");
    } else {
        ir->cond = (IRCond)(condbits^15);
    }

    if (numoperands < 0) {
        return NULL;
    }
    for (i = 0; i < numoperands; i++) {
        Operand *op = 0;
        effects[i] = 0;
        // special case rep instruction
        if (gl_p2 && i == 0 && !opimm[i] && !strcmp(instr->name, "rep") && operands[0] && operands[0]->kind == AST_ADDROF) {
            op = CompileInlineOperand(irl, operands[i], &effects[i], 1);
        } else {
            op = CompileInlineOperand(irl, operands[i], &effects[i], opimm[i]);
        }
        if (!op) {
            return NULL;
        }
        switch (op->kind) {
        case REG_REG:
        case REG_LOCAL:
            if (opimm[i]) {
                op = GetLea(irl, op);
            }
            break;
        case IMM_COG_LABEL:
            if (opimm[i] == 0) {
                effects[i] |= OPEFFECT_NOIMM;
            }
            break;
        default:
            break;
        }

        switch(i) {
        case 0:
            ir->dst = op;
            ir->dsteffect = (enum OperandEffect)effects[0];
            break;
        case 1:
            ir->src = op;
            ir->srceffect = (enum OperandEffect)effects[1];
            break;
        case 2:
            ir->src2 = op;
            break;
        default:
            ERROR(ast, "Too many operands to instruction");
            break;
        }
        if (op && op->kind == IMM_INT && (unsigned)op->val > (unsigned)511) {
            int ok = 0;
            if (instr->ops == CALL_OPERAND) {
                ok = 1;
            }
            else if (gl_p2) {
                /* check for ##; see ANY_BIG_IMM definition in outdat.c  */
                if (opimm[i] & 3) {
                    ok = 1;
                }
                if (instr->ops == P2_JUMP ||  instr->ops == P2_LOC || instr->ops == P2_CALLD) {
                    ok = 1;
                }
            }
            if (!ok) {
                ERROR(ast, "immediate operand %ld out of range", (long)op->val);
            }
        }
    }
    return ir;
}

static Operand *
FixupHereLabel(IRList *irl, IR *firstir, int addr, Operand *dst)
{
    IR *jir;

    addr += dst->val;
    if (addr < 0) {
        ERROR(NULL, "pc relative address $ - %lu in inline assembly is out of range",
              (unsigned long)-dst->val);
        return NewImmediate(0);
    }

    for (jir = firstir; jir; jir = jir->next) {
        if (jir->addr == addr) {
            Operand *newlabel = GetLabelOperand(NULL, false);
            IR *labelir = NewIR(OPC_LABEL);
            labelir->dst = newlabel;
            InsertAfterIR(irl, jir->prev, labelir);
            return newlabel;
        }
    }
    if (dst->val < 0) {
        ERROR(NULL, "pc relative address $ - %lu in inline assembly is out of range",
              (unsigned long)-dst->val);
    } else {
        ERROR(NULL, "pc relative address $ + %lu in inline assembly is out of range",
              (unsigned long)dst->val);
    }

    return NewImmediate(0);;
}

static void
CompileTraditionalInlineAsm(IRList *irl, AST *origtop, unsigned asmFlags)
{
    AST *ast;
    AST *top = origtop;
    unsigned relpc;
    IR *firstir;
    IR *fcache = NULL;
    IR *startlabel = NULL;
    IR *endlabel = NULL;
    IR *ir;
    IR *org0 = NULL;
    IR *orgh = NULL;
    Operand *enddst, *startdst;
    bool isConst = asmFlags & INLINE_ASM_FLAG_CONST;
    bool isInFcache = false;
    AsmState state[MAX_ASM_NEST] = { 0 };
    unsigned asmNest;

    asmNest = 0;
    state[asmNest].is_active = true;
    if (!curfunc) {
        ERROR(origtop, "Internal error, no context for inline assembly");
        return;
    }
    if (curfunc->code_placement != CODE_PLACE_HUB) {
        // never generate fcache stuff!
        asmFlags &= ~INLINE_ASM_FLAG_FCACHE;
    }

    enddst = NewHubLabel();

    if (asmFlags & INLINE_ASM_FLAG_FCACHE) {
        if (gl_fcache_size <= 0) {
            WARNING(origtop, "FCACHE is disabled, asm will be in HUB");
        } else {
            isInFcache = true;
            startdst = NewHubLabel();
            fcache = NewIR(OPC_FCACHE);
            fcache->src = startdst;
            fcache->dst = enddst;
            fcache->flags |= FLAG_KEEP_INSTR;
            startlabel = NewIR(OPC_LABEL);
            startlabel->dst = startdst;
            startlabel->flags |= FLAG_LABEL_NOJUMP;
            endlabel = NewIR(OPC_LABEL);
            endlabel->dst = enddst;
            endlabel->flags |= FLAG_LABEL_NOJUMP;
            if (!org0) {
                org0 = NewIR(OPC_ORG);
                org0->dst = NewImmediate(0);
            }
            if (!orgh) {
                orgh = NewIR(OPC_HUBMODE);
            }
        }
    }
    // first run through and define all the labels
    asmNest = 0;
    state[asmNest].is_active = true;
    while (top) {
        ast = top;
        top = top->right;
        while (ast && ast->kind == AST_COMMENTEDNODE) {
            ast = ast->left;
        }
        if (!ast) continue;
        if (ast->kind == AST_ASM_IF) {
            int val;
            bool wasActive = state[asmNest].is_active;
            asmNest++;
            if (asmNest == MAX_ASM_NEST) {
                ERROR(ast, "conditional assembly nested too deep");
                --asmNest;
            }
            if (!IsConstExpr(ast->left)) {
                ERROR(ast, "expression in conditional assembly must be constant");
                val = 0;
            } else {
                val = EvalConstExpr(ast->left);
            }
            if (val && wasActive) {
                state[asmNest].is_active = true;
                state[asmNest].needs_else = false;
            } else {
                state[asmNest].is_active = false;
                state[asmNest].needs_else = wasActive;
            }
        } else if (ast->kind == AST_ASM_ELSEIF) {
            int val;
            if (!IsConstExpr(ast->left)) {
                ERROR(ast, "expression in conditional assembly must be constant");
                val = 0;
            } else {
                val = EvalConstExpr(ast->left);
            }
            if (state[asmNest].needs_else && val) {
                state[asmNest].needs_else = false;
                state[asmNest].is_active = true;
            } else {
                state[asmNest].is_active = false;
            }
        } else if (ast->kind == AST_ASM_ENDIF) {
            if (asmNest == 0) {
                ERROR(ast, "conditional assembly endif without if");
            } else {
                --asmNest;
            }
        } else if (!state[asmNest].is_active) {
            /* do nothing, if'd out */
        } else if (ast->kind == AST_IDENTIFIER) {
            void *labelop = (void *)GetLabelOperand(ast->d.string, isInFcache);
            AddSymbol(&curfunc->localsyms, ast->d.string, SYM_LOCALLABEL, labelop, NULL);
        }
    }

    // now go back and emit code
    top = origtop;
    relpc = 0;
    if (fcache) {
        AppendIR(irl, fcache);
        AppendIR(irl, startlabel);
        if (gl_p2) {
            AppendIR(irl, org0);
        }
    }
    firstir = NULL;
    while(top) {
        ast = top;
        top = top->right;
        if (ast->kind == AST_LINEBREAK) {
            continue;
        }
        while (ast && ast->kind == AST_COMMENTEDNODE) {
            ast = ast->left;
        }
        if (!ast) {
            /* do nothing */
        } else if (ast->kind == AST_ASM_IF) {
            int val;
            bool wasActive = state[asmNest].is_active;
            asmNest++;
            if (asmNest == MAX_ASM_NEST) {
                ERROR(ast, "conditional assembly nested too deep");
                --asmNest;
            }
            if (!IsConstExpr(ast->left)) {
                ERROR(ast, "expression in conditional assembly must be constant");
                val = 0;
            } else {
                val = EvalConstExpr(ast->left);
            }
            if (val && wasActive) {
                state[asmNest].is_active = true;
                state[asmNest].needs_else = false;
            } else {
                state[asmNest].is_active = false;
                state[asmNest].needs_else = wasActive;
            }
        } else if (ast->kind == AST_ASM_ELSEIF) {
            int val;
            if (!IsConstExpr(ast->left)) {
                ERROR(ast, "expression in conditional assembly must be constant");
                val = 0;
            } else {
                val = EvalConstExpr(ast->left);
            }
            if (state[asmNest].needs_else && val) {
                state[asmNest].needs_else = false;
                state[asmNest].is_active = true;
            } else {
                state[asmNest].is_active = false;
            }
        } else if (ast->kind == AST_ASM_ENDIF) {
            if (asmNest == 0) {
                ERROR(ast, "conditional assembly endif without if");
            } else {
                --asmNest;
            }
        } else if (!state[asmNest].is_active) {
            /* do nothing, if'd out */
        } else if (ast->kind == AST_INSTRHOLDER) {
            IR *ir = CompileInlineInstr_only(irl, ast->left, isInFcache);
            if (!ir) break;

            IR *extrair = ir->next;

            if (extrair) {
                ir->next = NULL;
            }
            if (isConst) {
                ir->flags |= FLAG_KEEP_INSTR;
            }
            ir->addr = relpc;
            if (!firstir) firstir = ir;
            AppendIR(irl, ir);
            relpc++;
            if (ir->opc == OPC_REPEAT && !isConst) {
                WARNING(ast, "REP in inline assembly may interfere with optimization");
                isConst = true;
                ir->flags |= FLAG_KEEP_INSTR;
            }
            if (ir->opc == OPC_RET && !isInFcache) {
                //WARNING(ast, "ret instruction in inline asm converted to jump to end of asm");
                ReplaceOpcode(ir, OPC_JUMP);
                ir->dst = enddst;
                if (!endlabel) {
                    endlabel = NewIR(OPC_LABEL);
                    endlabel->dst = enddst;
                }
            }
            if (extrair) {
                if (extrair->opc == OPC_RET && !isInFcache) {
                    ReplaceOpcode(extrair, OPC_JUMP);
                    extrair->dst = enddst;
                    if (!endlabel) {
                        endlabel = NewIR(OPC_LABEL);
                        endlabel->dst = enddst;
                    }
                }
                AppendIR(irl, extrair);
                relpc++;
            }
        } else if (ast->kind == AST_IDENTIFIER) {
            Symbol *sym = FindSymbol(&curfunc->localsyms, ast->d.string);
            Operand *op;
            IR *ir;
            if (!sym || sym->kind != SYM_LOCALLABEL) {
                ERROR(ast, "%s is not a label or is multiply defined", ast->d.string);
                break;
            }
            if (!sym->v.ptr) {
                sym->v.ptr = GetLabelOperand(sym->our_name, isInFcache);
            }
            op = (Operand *)sym->v.ptr;
            ir = EmitLabel(irl, op);
            ir->addr = relpc;
            ir->flags |= FLAG_KEEP_INSTR;
            if (!firstir) firstir = ir;
        } else if (ast->kind == AST_LINEBREAK || ast->kind == AST_COMMENT || ast->kind == AST_SRCCOMMENT) {
            // do nothing
        } else if (ast->kind == AST_LONGLIST) {
            AST *list = ast->left;
            AST *item;
            Operand *op;
            int32_t val;
            IR *ir;
            int count = 1;
            while (list) {
                if (list->kind != AST_EXPRLIST) {
                    ERROR(list, "Expected list of items");
                    break;
                }
                item = list->left;
                list = list->right;
                if (item && item->kind == AST_ARRAYREF) {
                    // long x[y] kind of thing
                    val = EvalPasmExpr(item->left);
                    count = EvalConstExpr(item->right);
                } else {
                    val = EvalPasmExpr(item);
                }
                for (int j = 0; j < count; j++) {
                    op = NewOperand(IMM_INT, "", val);
                    ir = EmitOp1(irl, OPC_LONG, op);
                    relpc++;
                    if (isConst) {
                        ir->flags |= FLAG_KEEP_INSTR;
                    }
                }
            }
        } else if (ast->kind == AST_WORDLIST || ast->kind == AST_BYTELIST) {
            ERROR(ast, "declaring non-long sized variables inside inline assembly is not supported");
            break;
        } else if (ast->kind == AST_RES) {
            Operand *op;
            int val = EvalConstExpr(ast->left);
            op = NewOperand(IMM_INT, "", val);
            ir = EmitOp1(irl, OPC_RESERVE, op);
            ir->flags |= FLAG_KEEP_INSTR;
            relpc += val;
        } else if (ast->kind == AST_BRKDEBUG) {
            if (gl_output == OUTPUT_ASM) {
                int brkCode = AsmDebug_CodeGen(ast, OutAsm_DebugEval, (void *)irl);
                if (brkCode >= 0) {
                    Operand *op = NewImmediate(brkCode);
                    ir = EmitOp1(irl, OPC_BREAK, op);
                    ir->flags |= FLAG_KEEP_INSTR;
                }
            } else {
                WARNING(ast, "DEBUG ignored inside inline assembly");
            }
        } else {
            ERROR(ast, "inline assembly of this item not supported yet");
            break;
        }
    }
    if (fcache || endlabel) {
        IR *fitir;
        if (relpc > gl_fcache_size) {
            ERROR(origtop, "Inline assembly too large to fit in fcache");
        }
        if (fcache) {
            fitir = NewIR(OPC_FIT);
            fitir->dst = NewImmediate(gl_fcache_size);
            fitir->flags |= FLAG_USER_INSTR;
            AppendIR(irl, fitir);
        }
        AppendIR(irl, endlabel);
        if (fcache && gl_p2) {
            AppendIR(irl, orgh);
        }
    }
    // now fixup any relative addresses
    for (ir = firstir; ir; ir = ir->next) {
        if (!IsDummy(ir)) {
            if (ir->dst && ir->dst->kind == IMM_PCRELATIVE) {
                ir->dst = FixupHereLabel(irl, firstir, ir->addr, ir->dst);
            }
            if (ir->src && ir->src->kind == IMM_PCRELATIVE) {
                ir->src = FixupHereLabel(irl, firstir, ir->addr, ir->src);
            }
        }
        if (fcache) {
            ir->fcache = fcache->src;
        }
    }
}

struct CopyLocalData {
    bool copyTo;
    IRList *irl;
    AST *line;
};

static int copyLocal(Symbol *sym, void *arg)
{
    struct CopyLocalData *info = (struct CopyLocalData *)arg;
    IRList *irl = info->irl;
    unsigned offset = (sym->offset + 3U) / 4U;
    uint64_t usedMask = curfunc->localsUsedInAsm;
    AST *linesrc = info->line;
    
    if (usedMask & (1ULL<<offset)) {
        Operand *cogtmp;
        Operand *hubtmp;
        Operand *dst, *src;
        unsigned size;
        IR *ir;
        switch (sym->kind) {
        case SYM_LOCALVAR:
        case SYM_PARAMETER:
        case SYM_RESULT:
            size = TypeSize((AST *)sym->v.ptr);
            break;
        default:
            return 1;
        }

        hubtmp = CompileSymbolForFunc(irl, sym, curfunc, linesrc); 
        cogtmp = NewOperand(REG_HW, "0", offset + PASM_INLINE_ASM_VAR_BASE);
        cogtmp->size = hubtmp->size = size;
        if (info->copyTo) {
            dst = cogtmp; src = hubtmp;
        } else {
            dst = hubtmp; src = cogtmp;
        }
        ir = EmitMove(irl, dst, src, linesrc);
        ir->flags |= FLAG_KEEP_INSTR;
    }
    return 1;
}

static void
CompileMiniDatBlock(IRList *irl, AST *origtop, unsigned asmFlags)
{
    AST *top = origtop;
    IR *fcache;
    IR *startlabel, *endlabel;
    IR *ir, *fitir;
    Operand *enddst, *startdst;
    Flexbuf *fb = (Flexbuf *)malloc(sizeof(Flexbuf));
    Flexbuf *relocs = (Flexbuf *)malloc(sizeof(Flexbuf));
    struct CopyLocalData copyInfo;
    
    flexbuf_init(fb, 1024);
    flexbuf_init(relocs, 1024);

    startdst = NewHubLabel();
    enddst = NewHubLabel();
    fcache = NewIR(OPC_FCACHE);
    fcache->src = startdst;
    fcache->dst = enddst;
    fcache->flags |= FLAG_KEEP_INSTR;
    startlabel = NewIR(OPC_LABEL);
    startlabel->dst = startdst;
    startlabel->flags |= FLAG_LABEL_NOJUMP;
    endlabel = NewIR(OPC_LABEL);
    endlabel->dst = enddst;
    endlabel->flags |= FLAG_LABEL_NOJUMP;

    /* compile the inline assembly as a DAT block */
    curfunc->localsUsedInAsm = 0;
    AssignAddresses(&curfunc->localsyms, top, 0);
    PrintDataBlock(fb, top, NULL, relocs);

    /* copy the locals used in the inline assembly */
    copyInfo.irl = irl;
    copyInfo.copyTo = true;
    copyInfo.line = origtop;
    IterateOverSymbols(&curfunc->localsyms, copyLocal, (void *)&copyInfo);

    /* start the assembly block */
    AppendIR(irl, fcache);
    AppendIR(irl, startlabel);
    if (gl_p2) {
        IR *org0 = NewIR(OPC_ORG);
        org0->dst = NewImmediate(0);
        AppendIR(irl, org0);
    }
    Operand *op = NewOperand(IMM_BINARY, (const char *)fb, (intptr_t)relocs);
    ir = EmitOp2(irl, OPC_BINARY_BLOB, ModData(current)->datlabel, op);
    ir->src2 = (Operand *)current;
    
    /* finish off the block */
    fitir = NewIR(OPC_FIT);
    fitir->dst = NewImmediate(gl_fcache_size);
    fitir->flags |= FLAG_USER_INSTR;
    AppendIR(irl, fitir);
    AppendIR(irl, endlabel);
    if (gl_p2) {
        IR *orgh = NewIR(OPC_HUBMODE);
        AppendIR(irl, orgh);
    }
    
    /* copy back the locals */
    copyInfo.copyTo = false;
    IterateOverSymbols(&curfunc->localsyms, copyLocal, (void *)&copyInfo);
}

void
CompileInlineAsm(IRList *irl, AST *origtop, unsigned asmFlags)
{
    bool useMiniDat = true;
    
    if (curfunc->code_placement != CODE_PLACE_HUB) {
        // never generate fcache stuff!
        asmFlags &= ~INLINE_ASM_FLAG_FCACHE;
    }
    if (!(asmFlags & INLINE_ASM_FLAG_FCACHE)) {
        useMiniDat = false;
    } else if (curfunc->force_locals_to_stack) {
        useMiniDat = true;
    } else if (curfunc->optimize_flags & OPT_FASTASM) {
        useMiniDat = false;
    }
    if ( useMiniDat ) {
        if (gl_fcache_size <= 0) {
            WARNING(origtop, "FCACHE is disabled, asm will be in HUB");
        } else {
            CompileMiniDatBlock(irl, origtop, asmFlags);
            return;
        }
    }
    CompileTraditionalInlineAsm(irl, origtop, asmFlags);
 }
