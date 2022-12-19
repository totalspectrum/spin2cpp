/*
 * Spin to C/C++ converter
 * Copyright 2011-2022 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for handling functions
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
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
        AST *typ;
        Symbol *sym;
        if (list->kind != AST_LISTHOLDER) {
            ERROR(list, "Internal error, expected parameter list");
            return;
        }
        ast = list->left;
        if (ast->kind == AST_DECLARE_VAR) {
            ast = ast->right;
        }
        if (needcomma) {
            flexbuf_printf(f, ", ");
        }
        if (!ast) {
            ast = AstTempIdentifier("param");
        }
        if (ast->kind == AST_VARARGS) {
            flexbuf_printf(f, "intptr_t __vaargs");
        } else if (IsIdentifier(ast)) {
            sym = FindSymbol(&func->localsyms, ast->d.string);
            if (sym && sym->kind == SYM_PARAMETER && sym->v.ptr) {
                typ = (AST *)sym->v.ptr;
            } else {
                typ = ast_type_generic;
            }
            PrintType(f, typ, 0);
            CppPrintName(f, ast->d.string, 0);
        } else {
            // assume it's a type
            PrintType(f, ast, 0);
        }
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

    if ((gl_optimize_flags & OPT_REMOVE_UNUSED_FUNCS) && 0 == func->callSites) {
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
        PrintType(f, GetFunctionReturnType(func), 0);
        flexbuf_printf(f, "%s_", current->classname);
    } else {
        if (func->is_static) {
            flexbuf_printf(f, "static ");
        }
        PrintType(f, GetFunctionReturnType(func), 0);
        flexbuf_printf(f, "\t");
    }
    CppPrintName(f, func->name, 0);
    flexbuf_printf(f, "(");
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
PrintVarList(Flexbuf *f, AST *origtype, AST *ast, int flags)
{
    AST *decl;
    int needcomma = 0;
    int count = 0;
    AST *lasttype = NULL;
    int isfirst = 1;
    AST *curtype;
    while (ast != NULL) {
        if (ast->kind != AST_LISTHOLDER) {
            ERROR(ast, "Expected variable list element\n");
            return count;
        }
        decl = ast->left;
        curtype = origtype;
	if (!curtype) {
	    curtype = ExprType(decl);
	    if (!curtype) {
	      curtype = ast_type_generic;
	    }
	}
        if (curtype != lasttype) {
	    if (!isfirst) {
	        flexbuf_printf(f, ";");
		PrintNewline(f);
	    }
	    flexbuf_printf(f, "  ");
	    PrintType(f, curtype, flags);
	    flexbuf_printf(f, "\t");
	    needcomma = 0;
	    lasttype = curtype;
            if (curtype && curtype->kind == AST_PTRTYPE) {
                // force next line to have a new declaration
                lasttype = NULL;
            }
	}
        if (needcomma) {
            flexbuf_printf(f, ", ");
        }
        needcomma = 1;
	isfirst = 0;
        if (decl->kind == AST_LOCAL_IDENTIFIER) {
            decl = decl->right;
        }
        switch (decl->kind) {
        case AST_IDENTIFIER:
            CppPrintName(f, decl->d.string, 0);
            count++;
            break;
        case AST_ARRAYDECL:
            CppPrintName(f, decl->left->d.string, 0);
            flexbuf_printf(f, "[");
            if (gl_expand_constants) {
                flexbuf_printf(f, "%d", (int)EvalConstExpr(decl->right));
            } else {
                PrintExpr(f, SimpleOptimizeExpr(decl->right), PRINTEXPR_DEFAULT);
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
            PrintVarList(f, NULL, func->locals, LOCAL);
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
                    n = TypeSize((AST *)sym->v.ptr);
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
            PrintType(f, GetFunctionReturnType(func), 0);
            CppPrintName(f, func->resultexpr->d.string, 0);
            //flexbuf_printf(f, " = 0;");
            flexbuf_printf(f, ";");
            PrintNewline(f);
        } else if (func->resultexpr->kind == AST_EXPRLIST) {
            AST *id = func->resultexpr;
            while (id) {
                if (id->left && id->left->kind == AST_IDENTIFIER) {
                    flexbuf_printf(f, "  int32_t ");
                    CppPrintName(f, id->left->d.string, 0);
                    flexbuf_printf(f, ";");
                } else {
                    ERROR(id, "Internal error printing return exprlist");
                }
                id = id->right;
            }
            PrintNewline(f);
        } else if (func->resultexpr->kind == AST_INTEGER) {
            // default return 0
        } else {
            ERROR(func->resultexpr, "Internal error printing function");
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
            AST *typ = ExprType(v->left);
            flexbuf_printf(f, "%*c%s[%d] = ", indent, ' ', func->parmarray, offset);
            if (typ && !IsIntType(typ)) {
                flexbuf_printf(f, "(int32_t)");
            }
            flexbuf_printf(f, "%s;", v->left->d.string);
            PrintNewline(f);
            offset++;
        }
    }
}
 
static void PrintStatement(Flexbuf *f, AST *ast, int indent); /* forward declaration */

void
PrintStatementList(Flexbuf *f, AST *ast, int indent)
{
    while (ast) {
        if (ast->kind != AST_STMTLIST) {
            ERROR(ast, "Internal error, expected statement list, got %d",
                  ast->kind);
            return;
        }
        PrintStatement(f, ast->left, indent);
        ast = ast->right;
    }
}

#define USE_CASE 1
#define USE_IF 0

static void
PrintCaseItem(Flexbuf *f, AST *var, AST *ast, int indent, int how)
{
    AST *expr;
    AST *exprlist;
    
    if (how == USE_CASE) {
        exprlist = ast->left;
        while (exprlist) {
            if (exprlist->kind == AST_OTHER) {
                flexbuf_printf(f, "%*cdefault:", indent, ' ');
                PrintNewline(f);
            } else if (exprlist->kind == AST_EXPRLIST) {
                expr = exprlist->left;
                if (expr->kind == AST_OTHER) {
                    flexbuf_printf(f, "%*cdefault", indent, ' ');
                } else if (expr->kind == AST_STRING) {
                    int c;
                    const char *sptr = expr->d.string;
                    while ((c = *sptr++) != 0) {
                        flexbuf_printf(f, "%*ccase '%c'", indent, ' ', c);
                        if (*sptr) {
                            flexbuf_printf(f, ":");
                            PrintNewline(f);
                        }
                    }
                } else {
                    flexbuf_printf(f, "%*ccase ", indent, ' ');
                    PrintExpr(f, expr, PRINTEXPR_DEFAULT);
                }
                flexbuf_printf(f, ":");
                PrintNewline(f);
            } else {
                ERROR(ast, "Unexpected value in case item expression list");
            }
            exprlist = exprlist->right;
        }
        PrintStatementList(f, ast->right, indent+2);
        flexbuf_printf(f, "%*cbreak;", indent+2, ' ');
        PrintNewline(f);
    } else {
        flexbuf_printf(f, "if (");
        expr = TransformCaseExprList(var, ast->left);
        PrintBoolExpr(f, expr, PRINTEXPR_DEFAULT);
        flexbuf_printf(f, ") {");
        PrintNewline(f);
        PrintStatementList(f, ast->right, indent+2);
        flexbuf_printf(f, "%*c}", indent, ' ');
    }
}

static void
PrintCaseStmt(Flexbuf *f, AST *expr, AST *ast, int indent)
{
    int items = 0;
    int first = 1;
    int otherIsLast = 1;
    int noOther = 1;
    int allConst = 1;  /* true if all case item expressions are constant */
    AST *var;
    AST *ptr;
    AST *item;
    AST *itemexpr;
    AST *itemexprlist;
    
    // scan the case statement for special cases
    for (ptr = ast; ptr; ptr = ptr->right) {
        if (ptr->kind != AST_LISTHOLDER) {
            ERROR(ast, "Internal error in case list");
            return;
        }
        item = ptr->left;
        itemexprlist = item->left;
        while (itemexprlist) {
            itemexpr = itemexprlist->left;
            if (itemexprlist->kind == AST_OTHER) {
                otherIsLast = 1;
                noOther = 0;
            } else if (itemexpr->kind == AST_STRING) {
                otherIsLast = 0;
            } else {
                otherIsLast = 0;
                if (!IsConstExpr(itemexpr)) {
                    allConst = 0;
                }
            }
            itemexprlist = itemexprlist->right;
        }
    }

    if (allConst && (otherIsLast || noOther)) {
        // use switch/case for this
        flexbuf_printf(f, "%*c", indent, ' ');
        flexbuf_printf(f, "switch(");
        PrintExpr(f, expr, PRINTEXPR_DEFAULT);
        flexbuf_printf(f, ") {");
        PrintNewline(f);
        while (ast) {
            item = ast->left;
            PrintCaseItem(f, NULL, item, indent, USE_CASE);
            ast = ast->right;
        }
        flexbuf_printf(f, "%*c}", indent, ' ');
        PrintNewline(f);
        return;
    }
    
    if (expr->kind == AST_IDENTIFIER) {
        var = expr;
    } else if (expr->kind == AST_ASSIGN) {
        var = expr->left;
        flexbuf_printf(f, "%*c", indent, ' ');
        PrintAssign(f, var, expr->right, PRINTEXPR_DEFAULT);
        flexbuf_printf(f, ";"); PrintNewline(f);
    } else {
        ERROR(expr, "Internal error, expected identifier or assignment in case");
        var = NULL;
    }
    while (ast) {
        if (first) {
            flexbuf_printf(f, "%*c ", indent, ' ');
            first = 0;
        } else {
            flexbuf_printf(f, " else ");
        }
        PrintCaseItem(f, var, ast->left, indent, USE_IF);
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
            PrintLookupArray(f, decl->right, PRINTEXPR_DEFAULT);
            flexbuf_printf(f, "};"); PrintNewline(f);
        } else {
            ERROR(ast, "internal error in expression re-arranging");
            return;
        }
    }
}

/*
 * print inline assembly
 */
static void
PrintInlineAsm(Flexbuf *f, AST *top, int indent)
{
    AST *ast;
    CppInlineState *state;

    state = calloc(sizeof(*state), 1);
    flexbuf_printf(f, "%*c__asm__ volatile(\n", indent, ' ');
    state->indent = indent + 4;
    while (top) {
        ast = top;
        top = top->right;
        while (ast && ast->kind == AST_COMMENTEDNODE) {
            ast = ast->left;
        }
        if (ast->kind == AST_INSTRHOLDER) {
            outputGasInstruction(f, ast->left, 1, state);
        } else {
            ERROR(ast, "Inline assembly of this item not supported yet");
            break;
        }
    }
    /* print outputs */
    flexbuf_printf(f, "%*c:", indent, ' ');
    for (ast = state->outputs; ast; ast = ast->right) {
        const char *name = GetUserIdentifierName(ast->left);
        flexbuf_printf(f, " [%s] \"+r\"(%s)", name, name);
        if (ast->right) {
            flexbuf_printf(f, ",");
        } else {
            flexbuf_printf(f, "\n");
        }
    }

    /* print inputs */
    flexbuf_printf(f, "%*c:", indent, ' ');
    for (ast = state->inputs; ast; ast = ast->right) {
        const char *name = GetUserIdentifierName(ast->left);
        flexbuf_printf(f, " [%s] \"r\"(%s)", name, name);
        if (ast->right) {
            flexbuf_printf(f, ",");
        } else {
            flexbuf_printf(f, "\n");
        }
    }

    flexbuf_printf(f, "%*c);\n", indent, ' ');
    free(state);
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
    case AST_COMMENT:
        PrintIndentedComment(f, ast, indent);
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
            // extract the return value if it's buried in a sequence
            while (retval->kind == AST_SEQUENCE) {
                if (retval->right) {
                    PrintStatement(f, retval->left, indent);
                    retval = retval->right;
                } else {
                    retval = retval->left;
                }
            }
            flexbuf_printf(f, "%*creturn ", indent, ' ');
            if (retval->kind == AST_EXPRLIST) {
                int n = AstListLen(retval);
                flexbuf_printf(f, "MakeTuple%d__(", n);
                PrintExprList(f, retval, PRINTEXPR_DEFAULT, NULL);
                flexbuf_printf(f, ")");
            } else {
                PrintTypedExpr(f, GetFunctionReturnType(curfunc), retval, PRINTEXPR_DEFAULT);
            }
        } else {
            flexbuf_printf(f, "%*creturn", indent, ' ');
        }
        flexbuf_printf(f, ";"); PrintNewline(f);
        break;
    case AST_THROW:
        PrintDebugDirective(f, ast);
        flexbuf_printf(f, "%*cif (!abortChain__) abort();", indent, ' ');
        PrintNewline(f);
        flexbuf_printf(f, "%*cabortChain__->val =  ", indent, ' ');
        if (ast->left) {
            PrintExpr(f, ast->left, PRINTEXPR_DEFAULT);
        } else if (curfunc->resultexpr) {
            PrintExpr(f, curfunc->resultexpr, PRINTEXPR_DEFAULT);
        } else {
            PrintExpr(f, AstInteger(0), PRINTEXPR_DEFAULT);
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
    case AST_QUITLOOP:
    case AST_ENDCASE:
        PrintDebugDirective(f, ast);
        flexbuf_printf(f, "%*cbreak;", indent, ' ');
        PrintNewline(f);
        break;
    case AST_CONTINUE:
        PrintDebugDirective(f, ast);
        flexbuf_printf(f, "%*ccontinue;", indent, ' ');
        PrintNewline(f);
        break;
    case AST_GOTO:
        PrintDebugDirective(f, ast);
        flexbuf_printf(f, "%*cgoto ", indent, ' ');
        CppPrintName(f, ast->left->d.string, 0);
        flexbuf_printf(f, ";");
        PrintNewline(f);
        break;
    case AST_LABEL:
        PrintDebugDirective(f, ast);
        flexbuf_printf(f, "%*c", indent, ' ');
        CppPrintName(f, ast->left->d.string, 0);
        /* the semicolon prevents errors if the label is at the end
           of a compound statement */
        flexbuf_printf(f, ": ;");
        PrintNewline(f);
        break;
    case AST_IF:
        PrintDebugDirective(f, ast->left);
        flexbuf_printf(f, "%*cif (", indent, ' ');
        PrintBoolExpr(f, ast->left, PRINTEXPR_DEFAULT);
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
        PrintBoolExpr(f, ast->left, PRINTEXPR_DEFAULT);
        flexbuf_printf(f, ") {"); PrintNewline(f);
        PrintStatementList(f, ast->right, indent+2);
        flexbuf_printf(f, "%*c}", indent, ' ');
        PrintNewline(f);
        break;
    case AST_FOR:
    case AST_FORATLEASTONCE:
        PrintDebugDirective(f, ast);
        flexbuf_printf(f, "%*cfor(", indent, ' ');
        if (ast->left) {
            PrintExpr(f, ast->left, PRINTEXPR_TOPLEVEL);
        }
        flexbuf_printf(f, "; ");
        ast = ast->right;
        if (ast->left) {
            PrintBoolExpr(f, ast->left, PRINTEXPR_DEFAULT);
        }
        flexbuf_printf(f, "; ");
        ast = ast->right;
        if (ast->left) {
            PrintExpr(f, ast->left, PRINTEXPR_TOPLEVEL);
        }
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
        PrintBoolExpr(f, ast->left, PRINTEXPR_DEFAULT);
        flexbuf_printf(f, ");");
        PrintNewline(f);
        break;
    case AST_COUNTREPEAT:
        ERROR(ast, "Internal error, unexpected COUNTREPEAT");
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
        PrintDebugDirective(f, ast);
        PrintInlineAsm(f, ast->left, indent);
        break;
    case AST_SEQUENCE:
        PrintDebugDirective(f, ast);
        PrintStatement(f, ast->left, indent);
        PrintStatement(f, ast->right, indent);
        break;
    default:
    case AST_ASSIGN:
        PrintDebugDirective(f, ast);
        if (ast->kind == AST_ASSIGN && ast->left->kind == AST_EXPRLIST) {
            // multiple assignment
            AST *lhs = ast->left;
            AST *rhs = ast->right;
            int n = AstListLen(lhs);
            // if the rhs is a sequence, try to distill it down
            while (rhs && rhs->kind == AST_SEQUENCE) {
                if (rhs->right) {
                    PrintStatement(f, rhs->left, indent);
                    rhs = rhs->right;
                } else {
                    rhs = rhs->left;
                }
            }
            flexbuf_printf(f, "%*c", indent, ' ');
            flexbuf_printf(f, "{ Tuple%d__ tmp__ = ", n);
            if (rhs && rhs->kind == AST_EXPRLIST) {
                flexbuf_printf(f, "MakeTuple%d__(", n, n);
                PrintExprList(f, rhs, PRINTEXPR_DEFAULT, NULL);
                flexbuf_printf(f, ")");
            } else {
                PrintExpr(f, rhs, PRINTEXPR_DEFAULT);
            }
            flexbuf_printf(f, "; ");
            n = 0;
            while (lhs) {
                if (lhs->left && lhs->left->kind != AST_EMPTY) {
                    PrintLHS(f, lhs->left, PRINTEXPR_ASSIGNMENT);
                    flexbuf_printf(f, " = tmp__.v%d; ", n);
                }
                n++;
                lhs = lhs->right;
            }
            flexbuf_printf(f, " }");
        } else if (ast->kind == AST_ASSIGN) {
            flexbuf_printf(f, "%*c", indent, ' ');
            PrintAssign(f, ast->left, ast->right, PRINTEXPR_DEFAULT);
        } else {
            flexbuf_printf(f, "%*c", indent, ' ');
            PrintExpr(f, ast, PRINTEXPR_DEFAULT);
        }
        flexbuf_printf(f, ";");
        PrintNewline(f);
        break;
    }
}

static void
PrintFunctionStmts(Flexbuf *f, Function *func)
{
    assert(curfunc == func);
    PrintStatementList(f, func->body, 2);
}


void
PrintFunctionBodies(Flexbuf *f, Module *parse)
{
    Function *pf;

    for (pf = parse->functions; pf; pf = pf->next) {
        if (pf->body && pf->body->kind == AST_STRING) {
            // external declaration
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
        if ((gl_optimize_flags & OPT_REMOVE_UNUSED_FUNCS) && 0 == pf->callSites) {
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
            PrintType(f, GetFunctionReturnType(pf), 0);
            flexbuf_printf(f, "%s_", parse->classname);

        } else {
            PrintType(f, GetFunctionReturnType(pf), 0);
            flexbuf_printf(f, "%s::", parse->classname);
        }
        CppPrintName(f, pf->name, 0);
        flexbuf_printf(f, "(");
        PrintParameterList(f, pf);
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
