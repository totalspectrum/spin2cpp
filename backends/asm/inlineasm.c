#include "spinc.h"
#include "outasm.h"

Operand *
GetLabelOperand(const char *name)
{
    Operand *op;
#if 1
    name = NewTempLabelName();
#else    
    if (curfunc) {
        name = IdentifierLocalName(curfunc, name);
    }
#endif    
    if (curfunc && curfunc->code_placement == CODE_PLACE_HUB) {
        op = NewOperand(IMM_HUB_LABEL, name, 0);
    } else {
        op = NewOperand(IMM_COG_LABEL, name, 0);
    }
    return op;
}

Operand *
GetLabelFromSymbol(AST *where, const char *name)
{
    Symbol *sym;
    sym = FindSymbol(&curfunc->localsyms, name);
    if (!sym || sym->kind != SYM_LOCALLABEL) {
        ERROR(where, "%s is not a label in this function", name);
        return NULL;
    }
    if (!sym->val) {
        sym->val = (void *)GetLabelOperand(name);
    }
    return (Operand *)sym->val;
}

extern void ValidateStackptr(void);
extern Operand *stackptr;
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
                r = CompileIdentifier(irl, expr);
                if (!r) {
                    ERROR(expr, "Bad identifier expression %s", sym->user_name);
                    return NewImmediate(0);
                }
                r_address = immflag;
                if (r->kind == HUBMEM_REF) {
                    ERROR(expr, "Variable %s must be placed in memory (probably due to an @ expression) and hence cannot be accessed in inline assembly", sym->user_name);
                }
                break;
            case SYM_CONSTANT:
                v = EvalPasmExpr(expr);
                if (!immflag) {
                    WARNING(expr, "symbol %s used without #", sym->user_name);
                }
                r = ImmediateRef(immflag, v);
                break;
            case SYM_LOCALLABEL:
                if (!immflag) {
                    ERROR(expr, "must use an immediate with labels in inline asm");
                }
                r = GetLabelFromSymbol(expr, sym->our_name);
                immflag = 0;
                break;
            case SYM_LABEL:
                if (!immflag) {
                    ERROR(expr, "must use an immediate with global labels in inline asm");
                }
                r = LabelRef(irl, sym);
                break;
            case SYM_HWREG:
            {
                HwReg *hw = sym->val;
                r = GetOneGlobal(REG_HW, hw->name, 0);
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
        HwReg *hw = expr->d.ptr;
        return GetOneGlobal(REG_HW, hw->name, 0);
    } else if (expr->kind == AST_CATCH) {
        r = CompileInlineOperand(irl, expr->left, effects, 0);
        if (r && effects) {
            *effects |= OPEFFECT_FORCEABS;
        }
        return r;
    } else if (expr->kind == AST_HERE) {
        /* handle $ */
        return NewPcRelative(0);
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
            if (expr->left && expr->left->kind == AST_ARRAYREF && IsConstExpr(expr->left->right) && IsConstExpr(expr->right)) {
                int offset = EvalConstExpr(expr->left->right);
                offset = offset + sign * EvalConstExpr(expr->right);
                r = CompileInlineOperand(irl, expr->left->left, effects, 0);
                r = SubRegister(r, offset * LONG_SIZE);
                return r;
            }
        }
        if (IsConstExpr(expr)) {
            int x = EvalPasmExpr(expr);
            return ImmediateRef(immflag, x);
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
CompileInlineInstr(IRList *irl, AST *ast)
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
    // check for conditional execution
    if (gl_p2) {
        condbits = ival >> 28;
    } else {
        condbits = (ival >> 18) & 0xf;
    }
    switch (condbits) {
    case 0xf:
        ir->cond = COND_TRUE;
        break;
    case 0xe:
        ir->cond = COND_LE;
        break;
    case 0xc:
        ir->cond = COND_LT;
        break;
    case 0xa:
        ir->cond = COND_EQ;
        break;
    case 0x5:
        ir->cond = COND_NE;
        break;
    case 0x3:
        ir->cond = COND_GE;
        break;
    case 0x1:
        ir->cond = COND_GT;
        break;
    default:
        ERROR(ast, "Cannot handle this condition on instruction in inline asm");
        break;
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
        switch(i) {
        case 0:
            ir->dst = op;
            ir->dsteffect = effects[0];
            break;
        case 1:
            ir->src = op;
            ir->srceffect = effects[1];
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
                ERROR(ast, "immediate operand %d out of range", op->val);
            }
        }
    }
    AppendIR(irl, ir);
    return ir;
}

static Operand *
FixupHereLabel(IRList *irl, IR *firstir, int addr, Operand *dst)
{
    IR *jir;

    addr += dst->val;
    if (addr < 0) {
        ERROR(NULL, "pc relative address $ - %u in inline assembly is out of range",
              -dst->val);
        return NewImmediate(0);
    }
    
    for (jir = firstir; jir; jir = jir->next) {
        if (jir->addr == addr) {
            Operand *newlabel = GetLabelOperand(NULL);
            IR *labelir = NewIR(OPC_LABEL);
            labelir->dst = newlabel;
            InsertAfterIR(irl, jir->prev, labelir);
            return newlabel;
        }
    }
    if (dst->val < 0) {
        ERROR(NULL, "pc relative address $ - %u in inline assembly is out of range",
              -dst->val);
    } else {
        ERROR(NULL, "pc relative address $ + %u in inline assembly is out of range",
              dst->val);
    }
        
    return NewImmediate(0);;
}

void
CompileInlineAsm(IRList *irl, AST *origtop, unsigned asmFlags)
{
    AST *ast;
    AST *top = origtop;
    unsigned relpc;
    IR *firstir;
    IR *fcache = NULL;
    IR *startlabel = NULL;
    IR *endlabel = NULL;
    IR *ir;
    Operand *enddst, *startdst;
    bool isConst = asmFlags & INLINE_ASM_FLAG_CONST;
    
    if (!curfunc) {
        ERROR(origtop, "Internal error, no context for inline assembly");
        return;
    }

    if (asmFlags & INLINE_ASM_FLAG_FCACHE) {
        if (gl_fcache_size <= 0) {
            WARNING(origtop, "FCACHE is disabled, asm will be in HUB");
        } else {
            startdst = NewHubLabel();
            enddst = NewHubLabel();
            fcache = NewIR(OPC_FCACHE);
            fcache->src = startdst;
            fcache->dst = enddst;
            startlabel = NewIR(OPC_LABEL);
            startlabel->dst = startdst;
            startlabel->flags |= FLAG_LABEL_NOJUMP;
            endlabel = NewIR(OPC_LABEL);
            endlabel->dst = enddst;
            endlabel->flags |= FLAG_LABEL_NOJUMP;
        }
    }
    // first run through and define all the labels
    while (top) {
        ast = top;
        top = top->right;
        while (ast && ast->kind == AST_COMMENTEDNODE) {
            ast = ast->left;
        }
        if (ast->kind == AST_IDENTIFIER) {
            void *labelop = (void *)GetLabelOperand(ast->d.string);
            AddSymbol(&curfunc->localsyms, ast->d.string, SYM_LOCALLABEL, labelop, NULL);
        }
    }
    
    // now go back and emit code
    top = origtop;
    relpc = 0;
    if (fcache) {
        AppendIR(irl, fcache);
        AppendIR(irl, startlabel);
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
        if (ast->kind == AST_INSTRHOLDER) {
            IR *ir = CompileInlineInstr(irl, ast->left);
            if (!ir) break;
            if (isConst) {
                ir->flags |= FLAG_KEEP_INSTR;
            }
            ir->addr = relpc;
            if (!firstir) firstir = ir;
            relpc++;
        } else if (ast->kind == AST_IDENTIFIER) {
            Symbol *sym = FindSymbol(&curfunc->localsyms, ast->d.string);
            Operand *op;
            IR *ir;
            if (!sym || sym->kind != SYM_LOCALLABEL) {
                ERROR(ast, "%s is not a label or is multiply defined", ast->d.string);
                break;
            }
            if (!sym->val) {
                sym->val = GetLabelOperand(sym->our_name);
            }
            op = (Operand *)sym->val;
            ir = EmitLabel(irl, op);
            ir->addr = relpc;
            if (!firstir) firstir = ir;
        } else if (ast->kind == AST_LINEBREAK || ast->kind == AST_COMMENT || ast->kind == AST_SRCCOMMENT) {
            // do nothing
        } else {
            ERROR(ast, "inline assembly of this item not supported yet");
            break;
        }
    }
    if (fcache) {
        if (relpc > gl_fcache_size) {
            ERROR(origtop, "Inline assembly too large to fit in fcache");
        }
        AppendIR(irl, endlabel);
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
