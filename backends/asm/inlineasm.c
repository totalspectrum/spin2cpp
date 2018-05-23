#include "spinc.h"
#include "outasm.h"

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
	      ERROR(expr, "Unknown symbol %s", expr->d.string);
	      return NULL;
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
static void
CompileInlineInstr(IRList *irl, AST *ast)
{
    AST *sub;
    Instruction *instr = (Instruction *)ast->d.ptr;
    IR *ir = NewIR(instr->opc);
    int numoperands = 0;
    int expectops;
    ir->instr = instr;

    /* parse operands and put them in place */
    switch (instr->ops) {
    case NO_OPERANDS:
        expectops = 0;
        break;
    case TWO_OPERANDS:
    case TWO_OPERANDS_OPTIONAL:
    case JMPRET_OPERANDS:
    case P2_TJZ_OPERANDS:
    case P2_TWO_OPERANDS:
    case P2_RDWR_OPERANDS:
    case P2_LOC:
        expectops = 2;
        break;
    case THREE_OPERANDS_BYTE:
    case THREE_OPERANDS_WORD:
    case THREE_OPERANDS_NIBBLE:
        expectops = 3;
        break;
    default:
        expectops = 1;
        break;
    }
    // check for modifiers and operands
    sub = ast->right;
    while (sub != NULL) {
        if (sub->kind == AST_INSTRMODIFIER) {
            InstrModifier *mod = (InstrModifier *)sub->d.ptr;
	    if (!strcmp(mod->name, "wc")) {
		 ir->flags |= FLAG_WC;
	    } else if (!strcmp(mod->name, "wz")) {
		 ir->flags |= FLAG_WZ;
	    } else if (!strcmp(mod->name, "nr")) {
		 ir->flags |= FLAG_NR;
	    } else if (!strcmp(mod->name, "wr")) {
		 ir->flags |= FLAG_WR;
	    } else if (!strcmp(mod->name, "#")) {
//		 immflag = 1;
	    } else {
		 ERROR(ast, "Modifier %s not handled yet in inline asm", mod->name);
	    }
	} else if (sub->kind == AST_EXPRLIST) {
	     Operand *op;
	     op = CompileInlineOperand(irl, sub->left);
	     switch(numoperands) {
	     case 0:
		  ir->dst = op;
		  break;
	     case 1:
		  ir->src = op;
		  break;
	     default:
		  ERROR(sub, "Too many operands to instruction");
		  break;
	     }
	     numoperands++;
        } else {
            ERROR(ast, "Internal error parsing instruction");
        }
        sub = sub->right;
    }
    if (expectops != numoperands) {
        ERROR(ast, "Wrong number of operands to inline %s: expected %d, found %d", instr->name, expectops, numoperands);
    }
    AppendIR(irl, ir);
}

void
CompileInlineAsm(IRList *irl, AST *top)
{
    AST *ast;
    while(top) {
        ast = top;
        top = top->right;
        while (ast && ast->kind == AST_COMMENTEDNODE) {
            ast = ast->left;
        }
        if (ast->kind == AST_INSTRHOLDER) {
            CompileInlineInstr(irl, ast->left);
        } else {
            ERROR(ast, "inline assembly of this item not supported yet");
            break;
        }
    }
}

