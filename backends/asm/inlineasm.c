#include "spinc.h"
#include "outasm.h"

//
// compile an expression as an inine asm operand
//
static Operand *
CompileInlineOperand(IRList *irl, AST *expr)
{
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
	      return CompileIdentifier(irl, expr);
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
    int immflag = 0;
    ir->instr = instr;

    // check for modifiers and operands
    sub = ast->right;
    while (sub != NULL) {
        if (sub->kind == AST_INSTRMODIFIER) {
            InstrModifier *mod = sub->d.ptr;
	    if (!strcmp(mod->name, "wc")) {
		 ir->flags |= FLAG_WC;
	    } else if (!strcmp(mod->name, "wz")) {
		 ir->flags |= FLAG_WZ;
	    } else if (!strcmp(mod->name, "nr")) {
		 ir->flags |= FLAG_NR;
	    } else if (!strcmp(mod->name, "wr")) {
		 ir->flags |= FLAG_WR;
	    } else if (!strcmp(mod->name, "#")) {
		 immflag = 1;
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
    if (immflag) {
	 if (ir->src && ir->src->kind != IMM_INT) {
	   ERROR(sub, "Only immediate integers in inline asm");
	 }
    }
    AppendIR(irl, ir);
}

void
CompileInlineAsm(IRList *irl, AST *ast)
{
    while(ast) {
        if (ast->kind == AST_INSTRHOLDER) {
            CompileInlineInstr(irl, ast->left);
            ast = ast->right;
        } else {
            ERROR(ast, "inline assembly not supported yet");
            break;
        }
    }
}

