/*
 * Spin to C/C++ converter
 * Copyright 2011-2016 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for handling functions
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"
#include "outcpp.h"

static void
PrintParameterList(Flexbuf *f, Function *func)
{
    int needcomma = 0;
    AST *list = func->params;
    AST *ast;
    bool needSelf = false;

    needSelf = (gl_output == OUTPUT_C && !func->is_static) || func->force_static;
    if (!list && !needSelf) {
        flexbuf_printf(f, "void");
        return;
    }
    if (needSelf) {
        flexbuf_printf(f, "%s *self", func->module->classname);
        needcomma = 1;
    }
    while (list) {
        if (list->kind != AST_LISTHOLDER) {
            ERROR(list, "Internal error: expected parameter list");
            return;
        }
        ast = list->left;
        if (ast->kind != AST_IDENTIFIER) {
            ERROR(ast, "Internal error: expected identifier in function parameter list");
            return;
        }
        if (needcomma)
            flexbuf_printf(f, ", ");
        flexbuf_printf(f, "%s %s", gl_intstring, ast->d.string);
        needcomma = 1;
        list = list->right;
    }
}

void
PrintAnnotationList(Flexbuf *f, AST *ast, char terminal)
{
    while (ast) {
        if (ast->kind != AST_ANNOTATION) {
            ERROR(ast, "Internal error in function print: expecting annotation");
            return;
        }
        if (terminal == '\n') {
            flexbuf_printf(f, "%s", ast->d.string);
            PrintNewline(f);
        } else {
            flexbuf_printf(f, "%s%c", ast->d.string, terminal);
        }
        ast = ast->right;
    }
}

static void
PrintFunctionDecl(Flexbuf *f, Function *func, int isLocal)
{
    /* this may just be a placeholder for inline C++ code */
    if (!func->name)
        return;

    if ((gl_optimize_flags & OPT_REMOVE_UNUSED_FUNCS) && !func->is_used) {
        return;
    }

    /* make sure debug info (line number etc.) is up to date */
    PrintDebugDirective(f, func->decl);

    if (gl_output == OUTPUT_C && isLocal) {
        flexbuf_printf(f, "static");
    }
    flexbuf_printf(f, "  ");
    if (func->annotations) {
        PrintAnnotationList(f, func->annotations, ' ');
    }
    if (gl_output == OUTPUT_C) {
        PrintType(f, func->rettype);
        flexbuf_printf(f, " %s_%s(", current->classname, 
                func->name);
    } else {
        if (func->is_static) {
            flexbuf_printf(f, "static ");
        }
        PrintType(f, func->rettype);
        flexbuf_printf(f, "\t%s(", func->name);
    }
    PrintParameterList(f, func);
    flexbuf_printf(f, ");");
    PrintNewline(f);
}

int
PrintPublicFunctionDecls(Flexbuf *f, Module *parse)
{
    Function *pf;
    int n = 0;

    for (pf = parse->functions; pf; pf = pf->next) {
        if (!pf->is_public)
            continue;
        PrintFunctionDecl(f, pf, 0);
        n++;
    }
    return n;
}

int
PrintPrivateFunctionDecls(Flexbuf *f, Module *parse)
{
    Function *pf;
    int n = 0;

    for (pf = parse->functions; pf; pf = pf->next) {
        if (pf->is_public)
            continue;
        PrintFunctionDecl(f, pf, 1);
        n++;
    }
    return n;
}

/* returns the number of variables printed */
int
PrintVarList(Flexbuf *f, AST *typeast, AST *ast, int flags)
{
    AST *decl;
    int needcomma = 0;
    int count = 0;

    flexbuf_printf(f, "  ");
    if (flags & VOLATILE) {
        flexbuf_printf(f, "volatile ");
    }
    PrintType(f, typeast);
    flexbuf_printf(f, "\t");
    while (ast != NULL) {
        if (needcomma) {
            flexbuf_printf(f, ", ");
        }
        needcomma = 1;
        if (ast->kind != AST_LISTHOLDER) {
            ERROR(ast, "Expected variable list element\n");
            return count;
        }
        decl = ast->left;
        switch (decl->kind) {
        case AST_IDENTIFIER:
            flexbuf_printf(f, "%s", decl->d.string);
            count++;
            break;
        case AST_ARRAYDECL:
            flexbuf_printf(f, "%s[", decl->left->d.string);
            if (gl_expand_constants) {
                flexbuf_printf(f, "%d", (int)EvalConstExpr(decl->right));
            } else {
                PrintExpr(f, decl->right);
            }
            flexbuf_printf(f, "]");
            count++;
            break;
        case AST_ANNOTATION:
            flexbuf_printf(f, "%s ", decl->d.string);
            needcomma = 0;
            break;
        default:
            ERROR(decl, "Internal problem in variable list: type=%d\n", decl->kind);
            break;
        }
        ast = ast->right;
    }
    flexbuf_printf(f, ";");
    PrintNewline(f);
    return count;
}

/*
 * print variables in a function
 */
static void
PrintFunctionVariables(Flexbuf *f, Function *func)
{
    AST *v;
    int offset = 0;

    if (func->locals) {
        if (func->localarray && func->localarray == func->parmarray) {
            /* nothing to do here */
        } else if (func->localarray && func->localarray_len > 0) {
            flexbuf_printf(f, "  %s %s[%d];", gl_intstring, func->localarray, func->localarray_len);
            PrintNewline(f);
        } else {
            PrintVarList(f, ast_type_long, func->locals, LOCAL);
        }
    }
    if (func->parmarray) {
        int n;
        int parmsiz;

        parmsiz = AstListLen(func->params);
        if (func->result_in_parmarray) {
            parmsiz++;
        }
        if (func->localarray == func->parmarray) {
            Symbol *sym;
            offset = parmsiz;
            for (v = func->locals; v; v = v->right) {
                sym = VarSymbol(func, v->left);
                if (sym) {
		    sym->offset += (offset*4);
                    n = TypeSize((AST *)sym->val);
                    while (n > 0) {
                        n -= 4;
                        parmsiz++;
                    }
                }
            }
        }

        flexbuf_printf(f, "  %s %s[%d];", gl_intstring, func->parmarray, parmsiz);
        PrintNewline(f);
    }
    if (!func->result_in_parmarray && func->resultexpr) {
        if (func->resultexpr->kind == AST_IDENTIFIER) {
            flexbuf_printf(f, "  ");
            PrintType(f, func->rettype);
            flexbuf_printf(f, " %s = 0;", func->resultexpr->d.string);
            PrintNewline(f);
        }
    }
    /* now actually assign initial values for the array */
    if (func->parmarray) {
        int indent = 2;
        offset = 0;

        if (func->result_in_parmarray) {
            flexbuf_printf(f, "%*c%s[%d] = 0;", indent, ' ', func->parmarray, offset);
            offset++;
            PrintNewline(f);
        }
        for (v = func->params; v; v = v->right) {
            flexbuf_printf(f, "%*c%s[%d] = ", indent, ' ', func->parmarray, offset);
            flexbuf_printf(f, "%s;", v->left->d.string);
            PrintNewline(f);
            offset++;
        }
    }
}
 
static void PrintStatement(Flexbuf *f, AST *ast, int indent); /* forward declaration */

static void
PrintStatementList(Flexbuf *f, AST *ast, int indent)
{
    while (ast) {
        if (ast->kind != AST_STMTLIST) {
            ERROR(ast, "Internal error: expected statement list, got %d",
                  ast->kind);
            return;
        }
        PrintStatement(f, ast->left, indent);
        ast = ast->right;
    }
}

static void
PrintCaseItem(Flexbuf *f, AST *var, AST *ast, int indent)
{
    AST *expr;
    flexbuf_printf(f, "(");
    expr = TransformCaseExprList(var, ast->left);
    PrintBoolExpr(f, expr);
    flexbuf_printf(f, ") {");
    PrintNewline(f);
    PrintStatementList(f, ast->right, indent);
    flexbuf_printf(f, "%*c}", indent, ' ');
}

static void
PrintCaseStmt(Flexbuf *f, AST *expr, AST *ast, int indent)
{
    int items = 0;
    int first = 1;
    AST *var;

    if (expr->kind == AST_IDENTIFIER) {
        var = expr;
    } else if (expr->kind == AST_ASSIGN) {
        var = expr->left;
        flexbuf_printf(f, "%*c", indent, ' ');
        PrintAssign(f, var, expr->right);
        flexbuf_printf(f, ";"); PrintNewline(f);
    } else {
        ERROR(expr, "Internal error: expected identifier or assignment in case");
        var = NULL;
    }
    while (ast) {
        if (ast->kind != AST_LISTHOLDER) {
            ERROR(ast, "Internal error in case list");
            return;
        }
        if (first) {
            flexbuf_printf(f, "%*cif ", indent, ' ');
            first = 0;
        } else {
            flexbuf_printf(f, " else if ");
        }
        PrintCaseItem(f, var, ast->left, indent);
        ast = ast->right;
        items++;
    }
    PrintNewline(f);
}

/*
 * print extra declarations for function
 */
static void
PrintExtraDecl(Flexbuf *f, AST *ast, int indent)
{
    AST *decl;
    AST *id;
    while (ast) {
        decl = ast->left;
        ast = ast->right;

        if (decl && decl->kind == AST_TEMPARRAYDECL) {
            AST *arraydef = decl->left;
            id = arraydef->left;
            flexbuf_printf(f, "%*cstatic %s %s[] = {", indent, ' ', gl_intstring, id->d.string);
            PrintLookupArray(f, decl->right);
            flexbuf_printf(f, "};"); PrintNewline(f);
        } else {
            ERROR(ast, "internal error in expression re-arranging");
            return;
        }
    }
}

/*
 * print a single statement
 */
static void
PrintStatement(Flexbuf *f, AST *ast, int indent)
{
    AST *comment = NULL;
    AST *retval;
    
    if (!ast) return;

    switch (ast->kind) {
    case AST_COMMENTEDNODE:
        PrintIndentedComment(f, ast->right, indent);
        PrintStatement(f, ast->left, indent);
        break;
    case AST_ANNOTATION:
        // pure C code to emit inline
        flexbuf_printf(f, "%s\n", ast->d.string);
        break;
    case AST_RETURN:
        retval = ast->left;
        if (!retval) {
            retval = curfunc->resultexpr;
        }
        PrintDebugDirective(f, ast);
        if (retval) {
            flexbuf_printf(f, "%*creturn ", indent, ' ');
            PrintExpr(f, retval);
        } else {
            flexbuf_printf(f, "%*creturn", indent, ' ');
        }
        flexbuf_printf(f, ";"); PrintNewline(f);
        break;
    case AST_ABORT:
        PrintDebugDirective(f, ast);
        flexbuf_printf(f, "%*cif (!abortChain__) abort();", indent, ' ');
        PrintNewline(f);
        flexbuf_printf(f, "%*cabortChain__->val =  ", indent, ' ');
        if (ast->left) {
            PrintExpr(f, ast->left);
        } else if (curfunc->resultexpr) {
            PrintExpr(f, curfunc->resultexpr);
        } else {
            PrintExpr(f, AstInteger(0));
        }
        flexbuf_printf(f, ";"); PrintNewline(f);
        flexbuf_printf(f, "%*clongjmp(abortChain__->jmp, 1);", indent, ' ');
        PrintNewline(f);
        break;
    case AST_YIELD:
        PrintDebugDirective(f, ast);
        flexbuf_printf(f, "%*cYield__();", indent, ' ');
        PrintNewline(f);
        break;
    case AST_QUIT:
        PrintDebugDirective(f, ast);
        flexbuf_printf(f, "%*cbreak;", indent, ' ');
        PrintNewline(f);
        break;
    case AST_NEXT:
        PrintDebugDirective(f, ast);
        flexbuf_printf(f, "%*ccontinue;", indent, ' ');
        PrintNewline(f);
        break;
    case AST_IF:
        PrintDebugDirective(f, ast->left);
        flexbuf_printf(f, "%*cif (", indent, ' ');
        PrintBoolExpr(f, ast->left);
        flexbuf_printf(f, ") {");
        PrintNewline(f);
        ast = ast->right;
        // NOTE:
        // if an AST_THENELSE has a comment attached, the comment applies
        // to the "else" part (for the "if" part we would have wrapped the
        // whole AST_IF in an AST_COMMENTEDNODE)
        if (ast->kind == AST_COMMENTEDNODE) {
            comment = ast->right;
            ast = ast->left;
        }
        if (ast->kind != AST_THENELSE) {
            ERROR(ast, "error parsing if/then/else: got unexpected type %d", ast->kind);
            return;
        }
        PrintStatementList(f, ast->left, indent+2);
        if (ast->right) {
            flexbuf_printf(f, "%*c} else {", indent, ' ');
            PrintNewline(f);
            if (comment) {
                PrintIndentedComment(f, comment, indent+2);
                comment = NULL;
            }
            PrintDebugDirective(f, ast->right);
            PrintStatementList(f, ast->right, indent+2);
        }
        flexbuf_printf(f, "%*c}", indent, ' ');
        PrintNewline(f);
        break;
    case AST_WHILE:
        PrintDebugDirective(f, ast->left);
        flexbuf_printf(f, "%*cwhile (", indent, ' ');
        PrintBoolExpr(f, ast->left);
        flexbuf_printf(f, ") {"); PrintNewline(f);
        PrintStatementList(f, ast->right, indent+2);
        flexbuf_printf(f, "%*c}", indent, ' ');
        PrintNewline(f);
        break;
    case AST_FOR:
    case AST_FORATLEASTONCE:
        PrintDebugDirective(f, ast);
        flexbuf_printf(f, "%*cfor(", indent, ' ');
        PrintExprToplevel(f, ast->left);
        flexbuf_printf(f, "; ");
        ast = ast->right;
        PrintBoolExpr(f, ast->left);
        flexbuf_printf(f, "; ");
        ast = ast->right;
        PrintExprToplevel(f, ast->left);
        flexbuf_printf(f, ") {"); PrintNewline(f);
        PrintStatementList(f, ast->right, indent+2);
        flexbuf_printf(f, "%*c}", indent, ' ');
        PrintNewline(f);
        break;
    case AST_DOWHILE:
        flexbuf_printf(f, "%*cdo {", indent, ' ');
        PrintNewline(f);
        PrintStatementList(f, ast->right, indent+2);
        flexbuf_printf(f, "%*c} while (", indent, ' ');
        PrintBoolExpr(f, ast->left);
        flexbuf_printf(f, ");");
        PrintNewline(f);
        break;
    case AST_COUNTREPEAT:
        ERROR(ast, "Internal error: unexpected COUNTREPEAT");
        //PrintCountRepeat(f, ast, indent);
        break;
    case AST_STMTLIST:
        PrintStatementList(f, ast, indent);
        break;
    case AST_CASE:
        PrintDebugDirective(f, ast);
        PrintCaseStmt(f, ast->left, ast->right, indent);
        break;
    case AST_INLINEASM:
        ERROR(ast, "C/C++ output cannot handle inline assembly yet");
        break;
    case AST_SEQUENCE:
        PrintDebugDirective(f, ast);
        PrintStatement(f, ast->left, indent);
        PrintStatement(f, ast->right, indent);
        break;
    default:
    case AST_ASSIGN:
        PrintDebugDirective(f, ast);
        flexbuf_printf(f, "%*c", indent, ' ');
        if (ast->kind == AST_ASSIGN) {
            PrintAssign(f, ast->left, ast->right);
        } else {
            PrintExpr(f, ast);
        }
        flexbuf_printf(f, ";");
        PrintNewline(f);
        break;
    }
}

static void
PrintFunctionStmts(Flexbuf *f, Function *func)
{
    PrintStatementList(f, func->body, 2);
}


void
PrintFunctionBodies(Flexbuf *f, Module *parse)
{
    Function *pf;

    for (pf = parse->functions; pf; pf = pf->next) {
        PrintComment(f, pf->doccomment);
        if (pf->name == NULL) {
            PrintAnnotationList(f, pf->annotations, '\n');
            continue;
        } else if (gl_nospin) {
            /* skip all Spin methods */
            continue;
        }
        if ((gl_optimize_flags & OPT_REMOVE_UNUSED_FUNCS) && !pf->is_used) {
            flexbuf_printf(f, "// (function is not used)\n");
            continue;
        }
        curfunc = pf;
        PrintAnnotationList(f, curfunc->annotations, '\n');
        /* make sure debug info (line number etc.) is up to date */
        PrintDebugDirective(f, curfunc->decl);
        if (gl_output == OUTPUT_C) {
            if (!pf->is_public) {
                flexbuf_printf(f, "static ");
            }
            PrintType(f, pf->rettype);
            flexbuf_printf(f, " %s_%s(", parse->classname, pf->name);
            PrintParameterList(f, pf);

        } else {
            PrintType(f, pf->rettype);
            flexbuf_printf(f, " %s::%s(", parse->classname, pf->name);
            PrintParameterList(f, pf);
        }
        flexbuf_printf(f, ")");
        PrintNewline(f);
        flexbuf_printf(f, "{");
        PrintNewline(f);
        PrintFunctionVariables(f, pf);
        if (pf->extradecl) {
            PrintExtraDecl(f, pf->extradecl, 2);
            PrintNewline(f);
        }
        (void)PrintFunctionStmts(f, pf);

        flexbuf_printf(f, "}");
        PrintNewline(f);
        PrintNewline(f);
        curfunc = NULL;
    }
}
