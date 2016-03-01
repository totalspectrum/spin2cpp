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
PrintParameterList(FILE *f, Function *func)
{
    int needcomma = 0;
    AST *list = func->params;
    AST *ast;
    bool needSelf = false;

    needSelf = (gl_ccode && !func->is_static) || func->force_static;
    if (!list && !needSelf) {
        fprintf(f, "void");
        return;
    }
    if (needSelf) {
        fprintf(f, "%s *self", func->parse->classname);
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
            fprintf(f, ", ");
        fprintf(f, "int32_t %s", ast->d.string);
        needcomma = 1;
        list = list->right;
    }
}

void
PrintAnnotationList(FILE *f, AST *ast, char terminal)
{
    while (ast) {
        if (ast->kind != AST_ANNOTATION) {
            ERROR(ast, "Internal error in function print: expecting annotation");
            return;
        }
        if (terminal == '\n') {
            fprintf(f, "%s", ast->d.string);
            PrintNewline(f);
        } else {
            fprintf(f, "%s%c", ast->d.string, terminal);
        }
        ast = ast->right;
    }
}

static void
PrintFunctionDecl(FILE *f, Function *func, int isLocal)
{
    /* this may just be a placeholder for inline C++ code */
    if (!func->name)
        return;

    if ((gl_optimize_flags & OPT_REMOVE_UNUSED_FUNCS) && !func->is_used) {
        return;
    }

    /* make sure debug info (line number etc.) is up to date */
    PrintDebugDirective(f, func->decl);

    if (gl_ccode && isLocal) {
        fprintf(f, "static");
    }
    fprintf(f, "  ");
    if (func->annotations) {
        PrintAnnotationList(f, func->annotations, ' ');
    }
    if (gl_ccode) {
        PrintType(f, func->rettype);
        fprintf(f, " %s_%s(", current->classname, 
                func->name);
    } else {
        if (func->is_static) {
            fprintf(f, "static ");
        }
        PrintType(f, func->rettype);
        fprintf(f, "\t%s(", func->name);
    }
    PrintParameterList(f, func);
    fprintf(f, ");");
    PrintNewline(f);
}

int
PrintPublicFunctionDecls(FILE *f, Module *parse)
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
PrintPrivateFunctionDecls(FILE *f, Module *parse)
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
PrintVarList(FILE *f, AST *typeast, AST *ast, int flags)
{
    AST *decl;
    int needcomma = 0;
    int count = 0;

    fprintf(f, "  ");
    if (flags & VOLATILE) {
        fprintf(f, "volatile ");
    }
    PrintType(f, typeast);
    fprintf(f, "\t");
    while (ast != NULL) {
        if (needcomma) {
            fprintf(f, ", ");
        }
        needcomma = 1;
        if (ast->kind != AST_LISTHOLDER) {
            ERROR(ast, "Expected variable list element\n");
            return count;
        }
        decl = ast->left;
        switch (decl->kind) {
        case AST_IDENTIFIER:
            fprintf(f, "%s", decl->d.string);
            count++;
            break;
        case AST_ARRAYDECL:
            fprintf(f, "%s[", decl->left->d.string);
            if (gl_expand_constants) {
                fprintf(f, "%d", (int)EvalConstExpr(decl->right));
            } else {
                PrintExpr(f, decl->right);
            }
            fprintf(f, "]");
            count++;
            break;
        case AST_ANNOTATION:
            fprintf(f, "%s ", decl->d.string);
            needcomma = 0;
            break;
        default:
            ERROR(decl, "Internal problem in variable list: type=%d\n", decl->kind);
            break;
        }
        ast = ast->right;
    }
    fprintf(f, ";");
    PrintNewline(f);
    return count;
}

/*
 * print variables in a function
 */
static void
PrintFunctionVariables(FILE *f, Function *func)
{
    AST *v;
    int offset = 0;

    if (func->locals) {
        if (func->localarray && func->localarray == func->parmarray) {
            /* nothing to do here */
        } else if (func->localarray && func->localarray_len > 0) {
            fprintf(f, "  int32_t %s[%d];", func->localarray, func->localarray_len);
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

        fprintf(f, "  int32_t %s[%d];", func->parmarray, parmsiz);
        PrintNewline(f);
    }
    if (!func->result_in_parmarray && func->resultexpr) {
        if (func->resultexpr->kind == AST_IDENTIFIER) {
            fprintf(f, "  ");
            PrintType(f, func->rettype);
            fprintf(f, " %s = 0;", func->resultexpr->d.string);
            PrintNewline(f);
        }
    }
    /* now actually assign initial values for the array */
    if (func->parmarray) {
        int indent = 2;
        offset = 0;

        if (func->result_in_parmarray) {
            fprintf(f, "%*c%s[%d] = 0;", indent, ' ', func->parmarray, offset);
            offset++;
            PrintNewline(f);
        }
        for (v = func->params; v; v = v->right) {
            fprintf(f, "%*c%s[%d] = ", indent, ' ', func->parmarray, offset);
            fprintf(f, "%s;", v->left->d.string);
            PrintNewline(f);
            offset++;
        }
    }
}
 
static void PrintStatement(FILE *f, AST *ast, int indent); /* forward declaration */

static void
PrintStatementList(FILE *f, AST *ast, int indent)
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
PrintCaseExprList(FILE *f, AST *var, AST *ast)
{
    int needor = 0;
    while (ast) {
        if (needor) fprintf(f, " || ");
        if (ast->kind == AST_OTHER) {
            fprintf(f, "1");
        } else {
            if (ast->left->kind == AST_RANGE) {
                AST *betw;
                betw = NewAST(AST_ISBETWEEN, var, ast->left);
                PrintBoolExpr(f, betw);
            } else {
                PrintBoolExpr(f, AstOperator(T_EQ, var, ast->left));
            }
        }
        needor = 1;
        ast = ast->right;
    }
}

static void
PrintCaseItem(FILE *f, AST *var, AST *ast, int indent)
{
    fprintf(f, "(");
    PrintCaseExprList(f, var, ast->left);
    fprintf(f, ") {");
    PrintNewline(f);
    PrintStatementList(f, ast->right, indent);
    fprintf(f, "%*c}", indent, ' ');
}

static void
PrintCaseStmt(FILE *f, AST *expr, AST *ast, int indent)
{
    int items = 0;
    int first = 1;
    AST *var;

    if (expr->kind == AST_IDENTIFIER) {
        var = expr;
    } else {
        var = AstTempVariable(NULL);
        fprintf(f, "%*cint32_t %s = ", indent, ' ', var->d.string);
        PrintExpr(f, expr);
        fprintf(f, ";"); PrintNewline(f);
    }
    while (ast) {
        if (ast->kind != AST_LISTHOLDER) {
            ERROR(ast, "Internal error in case list");
            return;
        }
        if (first) {
            fprintf(f, "%*cif ", indent, ' ');
            first = 0;
        } else {
            fprintf(f, " else if ");
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
PrintExtraDecl(FILE *f, AST *ast, int indent)
{
    AST *decl;
    AST *id;
    while (ast) {
        decl = ast->left;
        ast = ast->right;

        if (decl && decl->kind == AST_TEMPARRAYDECL) {
            id = decl->left;
            fprintf(f, "%*cstatic int32_t %s[] = {", indent, ' ', id->d.string);
            PrintLookupArray(f, decl->right);
            fprintf(f, "};"); PrintNewline(f);
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
PrintStatement(FILE *f, AST *ast, int indent)
{
    AST *comment = NULL;
    AST *retval;
    
    if (!ast) return;

    switch (ast->kind) {
    case AST_COMMENTEDNODE:
        PrintIndentedComment(f, ast->right, indent);
        PrintStatement(f, ast->left, indent);
        break;
    case AST_RETURN:
        retval = ast->left;
        if (!retval) {
            retval = curfunc->resultexpr;
        }
        PrintDebugDirective(f, ast);
        if (retval) {
            fprintf(f, "%*creturn ", indent, ' ');
            PrintExpr(f, retval);
        } else {
            fprintf(f, "%*creturn", indent, ' ');
        }
        fprintf(f, ";"); PrintNewline(f);
        break;
    case AST_WAITCNT:
        fprintf(f, "%*cwaitcnt(", indent, ' ');
        PrintExpr(f, ast->left);
        fprintf(f, ");"); PrintNewline(f);
        break;
    case AST_WAITPEQ:
        fprintf(f, "%*cwaitpeq(", indent, ' ');
        PrintExprList(f, ast->left);
        fprintf(f, ");"); PrintNewline(f);
        break;
    case AST_WAITPNE:
        fprintf(f, "%*cwaitpne(", indent, ' ');
        PrintExprList(f, ast->left);
        fprintf(f, ");"); PrintNewline(f);
        break;
    case AST_WAITVID:
        fprintf(f, "%*cwaitpvid(", indent, ' ');
        PrintExprList(f, ast->left);
        fprintf(f, ");"); PrintNewline(f);
        break;        
    case AST_ABORT:
        PrintDebugDirective(f, ast);
        fprintf(f, "%*cif (!abortChain__) abort();", indent, ' ');
        PrintNewline(f);
        fprintf(f, "%*cabortChain__->val =  ", indent, ' ');
        if (ast->left) {
            PrintExpr(f, ast->left);
        } else {
            PrintExpr(f, curfunc->resultexpr);
        }
        fprintf(f, ";"); PrintNewline(f);
        fprintf(f, "%*clongjmp(abortChain__->jmp, 1);", indent, ' ');
        PrintNewline(f);
        break;
    case AST_YIELD:
        PrintDebugDirective(f, ast);
        fprintf(f, "%*cYield__();", indent, ' ');
        PrintNewline(f);
        break;
    case AST_QUIT:
        PrintDebugDirective(f, ast);
        fprintf(f, "%*cbreak;", indent, ' ');
        PrintNewline(f);
        break;
    case AST_NEXT:
        PrintDebugDirective(f, ast);
        fprintf(f, "%*ccontinue;", indent, ' ');
        PrintNewline(f);
        break;
    case AST_IF:
        PrintDebugDirective(f, ast->left);
        fprintf(f, "%*cif (", indent, ' ');
        PrintBoolExpr(f, ast->left);
        fprintf(f, ") {");
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
            fprintf(f, "%*c} else {", indent, ' ');
            PrintNewline(f);
            if (comment) {
                PrintIndentedComment(f, comment, indent+2);
                comment = NULL;
            }
            PrintDebugDirective(f, ast->right);
            PrintStatementList(f, ast->right, indent+2);
        }
        fprintf(f, "%*c}", indent, ' ');
        PrintNewline(f);
        break;
    case AST_WHILE:
        PrintDebugDirective(f, ast->left);
        fprintf(f, "%*cwhile (", indent, ' ');
        PrintBoolExpr(f, ast->left);
        fprintf(f, ") {"); PrintNewline(f);
        PrintStatementList(f, ast->right, indent+2);
        fprintf(f, "%*c}", indent, ' ');
        PrintNewline(f);
        break;
    case AST_FOR:
    case AST_FORATLEASTONCE:
        PrintDebugDirective(f, ast);
        fprintf(f, "%*cfor(", indent, ' ');
        PrintExprToplevel(f, ast->left);
        fprintf(f, "; ");
        ast = ast->right;
        PrintBoolExpr(f, ast->left);
        fprintf(f, "; ");
        ast = ast->right;
        PrintExprToplevel(f, ast->left);
        fprintf(f, ") {"); PrintNewline(f);
        PrintStatementList(f, ast->right, indent+2);
        fprintf(f, "%*c}", indent, ' ');
        PrintNewline(f);
        break;
    case AST_DOWHILE:
        fprintf(f, "%*cdo {", indent, ' ');
        PrintNewline(f);
        PrintStatementList(f, ast->right, indent+2);
        fprintf(f, "%*c} while (", indent, ' ');
        PrintBoolExpr(f, ast->left);
        fprintf(f, ");");
        PrintNewline(f);
        break;
    case AST_COUNTREPEAT:
        ERROR(ast, "Internal error: unexpected COUNTREPEAT");
        //PrintCountRepeat(f, ast, indent);
        break;
    case AST_STMTLIST:
        PrintStatementList(f, ast, indent+2);
        break;
    case AST_CASE:
        PrintDebugDirective(f, ast);
        PrintCaseStmt(f, ast->left, ast->right, indent);
        break;
    case AST_POSTEFFECT:
        PrintDebugDirective(f, ast);
        fprintf(f, "%*c", indent, ' ');
        PrintPostfix(f, ast, 1);
        fprintf(f, ";");
        PrintNewline(f);
        break;
    default:
    case AST_ASSIGN:
        PrintDebugDirective(f, ast);
        fprintf(f, "%*c", indent, ' ');
        if (ast->kind == AST_ASSIGN) {
            PrintAssign(f, ast->left, ast->right);
        } else {
            PrintExpr(f, ast);
        }
        fprintf(f, ";");
        PrintNewline(f);
        break;
    }
}

static void
PrintFunctionStmts(FILE *f, Function *func)
{
    PrintStatementList(f, func->body, 2);
}


void
PrintFunctionBodies(FILE *f, Module *parse)
{
    Function *pf;

    for (pf = parse->functions; pf; pf = pf->next) {
        if ((gl_optimize_flags & OPT_REMOVE_UNUSED_FUNCS) && !pf->is_used) {
            continue;
        }
        PrintComment(f, pf->doccomment);
        if (pf->name == NULL) {
            PrintAnnotationList(f, pf->annotations, '\n');
            continue;
        } else if (gl_nospin) {
            /* skip all Spin methods */
            continue;
        }
        curfunc = pf;
        PrintAnnotationList(f, curfunc->annotations, '\n');
        /* make sure debug info (line number etc.) is up to date */
        PrintDebugDirective(f, curfunc->decl);
        if (gl_ccode) {
            if (!pf->is_public) {
                fprintf(f, "static ");
            }
            PrintType(f, pf->rettype);
            fprintf(f, " %s_%s(", parse->classname, pf->name);
            PrintParameterList(f, pf);

        } else {
            PrintType(f, pf->rettype);
            fprintf(f, " %s::%s(", parse->classname, pf->name);
            PrintParameterList(f, pf);
        }
        fprintf(f, ")");
        PrintNewline(f);
        fprintf(f, "{");
        PrintNewline(f);
        PrintFunctionVariables(f, pf);
        if (pf->extradecl) {
            PrintExtraDecl(f, pf->extradecl, 2);
            PrintNewline(f);
        }
        (void)PrintFunctionStmts(f, pf);

        fprintf(f, "}");
        PrintNewline(f);
        PrintNewline(f);
        curfunc = NULL;
    }
}
