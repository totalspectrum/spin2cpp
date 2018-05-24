#include "spinc.h"
#include "outasm.h"

static Operand *
GetLabel(const char *name)
{
    Operand *op;
    if (curfunc && !curfunc->cog_code) {
        op = NewOperand(IMM_HUB_LABEL, name, 0);
    } else {
        op = NewOperand(IMM_COG_LABEL, name, 0);
    }
    return op;
}

//
// compile an expression as an inine asm operand
//
static Operand *
CompileInlineOperand(IRList *irl, AST *expr)
{
    Operand *r;
    int32_t v;
    if (expr->kind == AST_IMMHOLDER || expr->kind == AST_BIGIMMHOLDER) {
        expr = expr->left;
    }
    if (expr->kind == AST_IDENTIFIER) {
	 Symbol *sym = LookupSymbol(expr->d.string);
	 if (!sym) {
             ERROR(expr, "Undefined symbol %s", expr->d.string);
             return NewImmediate(0);
	 }
	 switch(sym->type) {
	 case SYM_PARAMETER:
	 case SYM_RESULT:
	 case SYM_LOCALVAR:
	 case SYM_TEMPVAR:
	      r = CompileIdentifier(irl, expr);
              if (!r) {
                  ERROR(expr, "Bad identifier expression %s", sym->name);
              }
              return r;
         case SYM_CONSTANT:
             v = EvalPasmExpr(expr);
             return NewImmediate(v);
         case SYM_LOCALLABEL:
             return (Operand *)sym->val;
             break;
	 default:
	      ERROR(expr, "Symbol %s is not usable in inline asm", sym->name);
	      return NULL;
	 }
    } else if (expr->kind == AST_INTEGER) {
         return NewImmediate(expr->d.ival);
    } else {
	 ERROR(expr, "Operand too complex for inline assembly");
	 return NULL;
    }
}

//
// compile an inline instruction
// ast points to an AST_INSTRUCTION
//
#define MAX_OPERANDS 4

static void
CompileInlineInstr(IRList *irl, AST *ast)
{
    Instruction *instr = (Instruction *)ast->d.ptr;
    IR *ir = NewIR(instr->opc);
    int numoperands;
    AST *operands[MAX_OPERANDS];
    uint32_t opimm[MAX_OPERANDS];
    int i;
    uint32_t effectFlags = 0;
    
    ir->instr = instr;

    /* parse operands and put them in place */
    numoperands = DecodeAsmOperands(instr, ast, operands, opimm, NULL, &effectFlags);

    ir->flags = effectFlags;
    
    if (numoperands < 0) {
        return;
    }
    if (numoperands == 3) {
        ERROR(ast, "Cannot yet handle instruction %s in inline asm", instr->name);
        return;
    }
    for (i = 0; i < numoperands; i++) {
        Operand *op;
        op = CompileInlineOperand(irl, operands[i]);
        switch(i) {
        case 0:
            ir->dst = op;
            break;
        case 1:
            ir->src = op;
            break;
        default:
            ERROR(ast, "Too many operands to instruction");
            break;
        }
    }
    AppendIR(irl, ir);
}

void
CompileInlineAsm(IRList *irl, AST *origtop)
{
    AST *ast;
    AST *top = origtop;

    if (!curfunc) {
        ERROR(origtop, "Internal error, no context for inline assembly");
        return;
    }
    
    // first run through and define all the labels
    while (top) {
        ast = top;
        top = top->right;
        while (ast && ast->kind == AST_COMMENTEDNODE) {
            ast = ast->left;
        }
        if (ast->kind == AST_IDENTIFIER) {
            Operand *labelop = GetLabel(ast->d.string);
            AddSymbol(&curfunc->localsyms, ast->d.string, SYM_LOCALLABEL, (void *)labelop);
        }
    }
    
    // now go back and emit code
    top = origtop;
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
            CompileInlineInstr(irl, ast->left);
        } else if (ast->kind == AST_IDENTIFIER) {
            Symbol *sym = FindSymbol(&curfunc->localsyms, ast->d.string);
            if (!sym || sym->type != SYM_LOCALLABEL) {
                ERROR(ast, "%s is not a label or is multiply defined", ast->d.string);
                break;
            }
            EmitLabel(irl, (Operand *)sym->val);
        } else {
            ERROR(ast, "inline assembly of this item not supported yet");
            break;
        }
    }
}

