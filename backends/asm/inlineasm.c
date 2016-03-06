#include "spinc.h"
#include "outasm.h"

//
// compile an inline instruction
// ast points to an AST_INSTRUCTION
static void
CompileInlineInstr(IRList *irl, AST *ast)
{
    AST *sub;
    Instruction *instr = (Instruction *)ast->d.ptr;
    IR *ir = NewIR(instr->opc);
    int immflag = 0;
    int numoperands = 0;
    
    ir->instr = instr;

    // check for modifiers and operands
    sub = ast->right;
    while (sub != NULL) {
        if (sub->kind == AST_INSTRMODIFIER) {
            InstrModifier *mod = sub->d.ptr;
        } else {
            ERROR(ast, "Internal error parsing instruction");
        }
        sub = sub->right;
    }
            
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

