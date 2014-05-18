/*
 * Spin to C/C++ converter
 * Copyright 2011-2014 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for handling functions
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

Function *curfunc;

Function *
NewFunction(void)
{
    Function *f;
    Function *pf;

    f = calloc(1, sizeof(*f));
    if (!f) {
        fprintf(stderr, "FATAL ERROR: Out of memory!\n");
        exit(1);
    }
    /* now link it into the current object */
    if (current->functions == NULL) {
        current->functions = f;
    } else {
        pf = current->functions;
        while (pf->next != NULL)
            pf = pf->next;
        pf->next = f;
    }
    f->parse = current;
    return f;
}

static Symbol *
EnterVariable(int kind, SymbolTable *stab, const char *name, AST *type)
{
    Symbol *sym;

    sym = AddSymbol(stab, name, kind, (void *)type);
    return sym;
}

int
EnterVars(int kind, SymbolTable *stab, void *symval, AST *varlist)
{
    AST *lower;
    AST *ast;
    Symbol *sym;
    int count = 0;
    int size;

    for (lower = varlist; lower; lower = lower->right) {
        if (lower->kind == AST_LISTHOLDER) {
            ast = lower->left;
            switch (ast->kind) {
            case AST_IDENTIFIER:
                sym = EnterVariable(kind, stab, ast->d.string, symval);
                sym->count = count++;
                break;
            case AST_ARRAYDECL:
                sym = EnterVariable(kind, stab, ast->left->d.string, NewAST(AST_ARRAYTYPE, symval, ast->right));
                size = EvalConstExpr(ast->right);
                sym->count = count;
                count += size;
                break;
            case AST_ANNOTATION:
                /* just ignore it */
                break;
            default:
                ERROR(ast, "Internal error: bad AST value %d", ast->kind);
                break;
            }
        } else {
            ERROR(lower, "Expected list of variables, found %d instead", lower->kind);
            return 0;
        }
    }
    return count;
}

/*
 * check for a variable having its address taken
 */
static bool
IsAddrRef(AST *body, Symbol *sym)
{
    if (body->kind == AST_ADDROF)
        return true;
    if (body->kind == AST_ARRAYREF && !IsArraySymbol(sym))
        return true;
    return false;
}

/*
 * scan a function body for various special conditions
 */
static void
ScanFunctionBody(Function *fdef, AST *body, AST *upper)
{
    AST *ast;
    Symbol *sym;

    if (!body)
        return;
    switch(body->kind) {
    case AST_ADDROF:
    case AST_ARRAYREF:
        /* see if it's a parameter whose address is being taken */
        ast = body->left;
        if (ast->kind == AST_IDENTIFIER) {
            sym = FindSymbol(&fdef->localsyms, ast->d.string);
            if (sym) {
                if (sym->type == SYM_PARAMETER) {
                    if (!fdef->parmarray)
                        fdef->parmarray = NewTemporaryVariable("_parm_");
                    fdef->localarray = fdef->parmarray;
                } else if (sym->type == SYM_LOCALVAR && IsAddrRef(body, sym) ) {
                    if (!fdef->localarray)
                        fdef->localarray = NewTemporaryVariable("_local_");
                }
            } else {
                /* Taking the address of an object variable? That will make the object volatile. */
                sym = FindSymbol(&current->objsyms, ast->d.string);
                if (sym && sym->type == SYM_VARIABLE && IsAddrRef(body, sym)) {
                    current->volatileVariables = 1;
                }
            }
        } else if (ast->kind == AST_RESULT) {
            /* hack to make F32.spin work: it expects all variables
               to be arranged as result,parameters,locals
               so force all of those into the _parm_ array
            */
            if (!fdef->parmarray) {
                fdef->parmarray = NewTemporaryVariable("_parm_");
            }
            fdef->localarray = fdef->parmarray;
            if (!fdef->result_in_parmarray) {
                fdef->result_in_parmarray = 1;
                fdef->resultexpr = NewAST(AST_RESULT, NULL, NULL);
                fdef->result_used = 1;
            }
        }
        break;
    case AST_IDENTIFIER:
        // convert plain foo into foo[0] if foo is an array
        sym = LookupSymbol(body->d.string);
        if (sym && IsArraySymbol(sym) && (sym->type == SYM_VARIABLE || sym->type == SYM_LOCALVAR)) 
        {
            if (upper && upper->kind != AST_ARRAYREF) {
                AST *deref = NewAST(AST_ARRAYREF, body, AstInteger(0));
                if (body == upper->left) {
                    upper->left = deref;
                } else if (body == upper->right) {
                    upper->right = deref;
                } else {
                    ERROR(body, "failed to dereference %s", body->d.string); 
                }
            }
        }
    default:
        break;
    }
    ScanFunctionBody(fdef, body->left, body);
    ScanFunctionBody(fdef, body->right, body);
}

/*
 * declare a function and create a Function structure for it
 */

void
DeclareFunction(int is_public, AST *funcdef, AST *body, AST *annotation, AST *comment)
{
    AST *funcblock;
    AST *holder;

    holder = NewAST(AST_FUNCHOLDER, funcdef, body);
    funcblock = NewAST(is_public ? AST_PUBFUNC : AST_PRIFUNC, holder, annotation);
    funcblock->d.ptr = (void *)comment;
    funcblock = NewAST(AST_LISTHOLDER, funcblock, NULL);

    current->funcblock = AddToList(current->funcblock, funcblock);
}

static void
doDeclareFunction(AST *funcblock)
{
    AST *holder;
    AST *funcdef;
    AST *body;
    AST *annotation;
    Function *fdef;
    AST *vars;
    AST *src;
    AST *comment;
    const char *resultname;
    int localcount;
    int is_public;

    is_public = (funcblock->kind == AST_PUBFUNC);
    holder = funcblock->left;
    annotation = funcblock->right;
    funcdef = holder->left;
    body = holder->right;
    comment = (AST *)funcblock->d.ptr;

    if (funcdef->kind != AST_FUNCDEF || funcdef->left->kind != AST_FUNCDECL) {
        ERROR(funcdef, "Internal error: bad function definition");
        return;
    }
    src = funcdef->left;
    if (src->left->kind != AST_IDENTIFIER) {
        ERROR(funcdef, "Internal error: no function name");
        return;
    }
    fdef = NewFunction();
    fdef->name = src->left->d.string;
    fdef->annotations = annotation;
    if (comment) {
        if (comment->kind != AST_COMMENT) {
            ERROR(comment, "Internal error: expected comment");
            abort();
        }
        fdef->doccomment = comment;
    }
    if (src->right && src->right->kind == AST_IDENTIFIER)
        resultname = src->right->d.string;
    else
        resultname = "result";
    fdef->resultexpr = AstIdentifier(resultname);
    fdef->is_public = is_public;
    fdef->type = ast_type_long;

    vars = funcdef->right;
    if (vars->kind != AST_FUNCVARS) {
        ERROR(vars, "Internal error: bad variable declaration");
    }

    /* enter the variables into the local symbol table */
    fdef->params = vars->left;
    fdef->locals = vars->right;
    EnterVars(SYM_PARAMETER, &fdef->localsyms, ast_type_long, fdef->params);
    localcount = EnterVars(SYM_LOCALVAR, &fdef->localsyms, ast_type_long, fdef->locals);

    AddSymbol(&fdef->localsyms, resultname, SYM_RESULT, ast_type_long);

    fdef->body = body;

    /* check for special conditions */
    ScanFunctionBody(fdef, body, NULL);

    /* if we put the locals into an array, record the size of that array */
    if (fdef->localarray)
        fdef->localarray_len = localcount;

    /* define the function itself */
    AddSymbol(&current->objsyms, fdef->name, SYM_FUNCTION, fdef);
}

void
DeclareFunctions(ParserState *P)
{
    AST *ast;

    ast = P->funcblock;
    while (ast) {
        doDeclareFunction(ast->left);
        ast = ast->right;
    }
}

/*
 * try to optimize lookup on constant arrays
 * if we can, modifies ast and returns any
 * declarations needed
 */
static AST *
ModifyLookup(AST *top)
{
    int len = 0;
    AST *ast;
    AST *expr;
    AST *ev;
    AST *table;
    AST *id;
    AST *decl;

    ev = top->left;
    table = top->right;
    if (table->kind == AST_TEMPARRAYUSE) {
        /* already modified, skip it */
        return NULL;
    }
    if (ev->kind != AST_LOOKEXPR || table->kind != AST_EXPRLIST) {
        ERROR(ev, "Internal error in lookup");
        return NULL;
    }
    /* see if the table is constant, and count the number of elements */
    ast = table;
    while (ast) {
        int c, d;
        expr = ast->left;
        ast = ast->right;

        if (expr->kind == AST_RANGE) {
            c = EvalConstExpr(expr->left);
            d = EvalConstExpr(expr->right);
            len += abs(d - c) + 1;
        } else {
            if (IsConstExpr(expr))
                len++;
            else
                return NULL;
        }
    }

    /* if we get here, the array is constant of length "len" */
    /* create a temporary identifier for it */
    id = AstTempVariable("look_");
    /* replace the table in the top expression */
    top->right = NewAST(AST_TEMPARRAYUSE, id, AstInteger(len));

    /* create a declaration */
    decl = NewAST(AST_TEMPARRAYDECL, id, table);

    /* put it in a list holder */
    decl = NewAST(AST_LISTHOLDER, decl, NULL);

    return decl;
}

/*
 * hook for optimization
 * there are a number of simple optimizations we can perform on a function
 * (1) Convert lookup/lookdown into constant array references
 * (2) Eliminate unused result variables
 *
 * Called recursively; the top level call has ast = func->body
 * Returns an AST that should be printed before the function body, e.g.
 * to declare temporary arrays.
 */
static AST *
Optimize(AST *ast, Function *func)
{
    AST *ldecl;
    AST *rdecl;

    if (!ast)
        return NULL;

    switch (ast->kind) {
    case AST_RETURN:
        if (!ast->left) {
            func->result_used = 1;
            return NULL;
        }
        return Optimize(ast->left, func);
    case AST_RESULT:
        func->result_used = 1;
        return NULL;
    case AST_IDENTIFIER:
        rdecl = func->resultexpr;
        if (rdecl && AstMatch(rdecl, ast))
            func->result_used = 1;
        return NULL;
    case AST_INTEGER:
    case AST_FLOAT:
    case AST_STRING:
    case AST_STRINGPTR:
    case AST_CONSTANT:
    case AST_HWREG:
    case AST_CONSTREF:
        return NULL;
    case AST_LOOKUP:
    case AST_LOOKDOWN:
        return ModifyLookup(ast);
    default:
        ldecl = Optimize(ast->left, func);
        rdecl = Optimize(ast->right, func);
        return AddToList(ldecl, rdecl);
    }
}

static void
PrintParameterList(FILE *f, AST *list)
{
    int needcomma = 0;
    AST *ast;

    if (!list) {
        fprintf(f, "void");
        return;
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
        fprintf(f, "%s%c", ast->d.string, terminal);
        ast = ast->right;
    }
}

static void
PrintFunctionDecl(FILE *f, Function *func, int isLocal)
{
    /* this may just be a placeholder for inline C++ code */
    if (!func->name)
        return;

    if (gl_ccode && isLocal) {
        fprintf(f, "static");
    }
    fprintf(f, "  ");
    if (func->annotations) {
        PrintAnnotationList(f, func->annotations, ' ');
    }
    if (gl_ccode) {
        fprintf(f, "int32_t %s_%s( %s *self", current->classname, 
                func->name, current->classname);
        if (func->params) {
            // more parameters coming
            fprintf(f, ", ");
        } else {
            // all done
            fprintf(f, " );\n");
            return;
        }
    } else {
        fprintf(f, "int32_t\t%s(", func->name);
    }
    PrintParameterList(f, func->params);
    fprintf(f, ");\n");
}

int
PrintPublicFunctionDecls(FILE *f, ParserState *parse)
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
PrintPrivateFunctionDecls(FILE *f, ParserState *parse)
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
            fprintf(f, "%s[%d]", decl->left->d.string,
                    (int)EvalConstExpr(decl->right));
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
    fprintf(f, ";\n");
    return count;
}

Symbol *VarSymbol(Function *func, AST *ast)
{
    if (ast && ast->kind == AST_ARRAYDECL)
        ast = ast->left;
    if (ast == 0 || ast->kind != AST_IDENTIFIER) {
        ERROR(ast, "internal error: expected variable name\n");
        return NULL;
    }
    return FindSymbol(&func->localsyms, ast->d.string);
}

/*
 * calculate the size of a type
 */
int typeSize(AST *ast)
{
    if (!ast)
        return 1;
    if (ast->kind == AST_ARRAYTYPE) {
        return typeSize(ast->left)*EvalConstExpr(ast->right);
    }
    if (ast->kind == AST_INTTYPE || ast->kind == AST_UNSIGNEDTYPE) {
        return EvalConstExpr(ast->left);
    }
    ERROR(ast, "internal error: bad type kind %d", ast->kind);
    return 0;
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
            fprintf(f, "  int32_t %s[%d];\n", func->localarray, func->localarray_len);
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
                    sym->count += offset;
                    n = typeSize((AST *)sym->val);
                    while (n > 0) {
                        n -= 4;
                        parmsiz++;
                    }
                }
            }
        }

        fprintf(f, "  int32_t %s[%d];\n", func->parmarray, parmsiz);
    }
    if (!func->result_in_parmarray) {
        if (func->resultexpr->kind == AST_IDENTIFIER)
            fprintf(f, "  int32_t %s = 0;\n", func->resultexpr->d.string);
    }
    /* now actually assign initial values for the array */
    if (func->parmarray) {
        int indent = 2;
        offset = 0;

        if (func->result_in_parmarray) {
            fprintf(f, "%*c%s[%d] = 0;\n", indent, ' ', func->parmarray, offset);
            offset++;
        }
        for (v = func->params; v; v = v->right) {
            fprintf(f, "%*c%s[%d] = ", indent, ' ', func->parmarray, offset);
            fprintf(f, "%s;\n", v->left->d.string);
            offset++;
        }
    }
}
 
static int PrintStatement(FILE *f, AST *ast, int indent); /* forward declaration */

static int
PrintStatementList(FILE *f, AST *ast, int indent)
{
    int sawreturn = 0;
    while (ast) {
        if (ast->kind != AST_STMTLIST) {
            ERROR(ast, "Internal error: expected statement list, got %d",
                  ast->kind);
            return 0;
        }
        sawreturn |= PrintStatement(f, ast->left, indent);
        ast = ast->right;
    }
    return sawreturn;
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
                AST *a, *b;
                a = ast->left->left;
                b = ast->left->right;
                fprintf(f, "Between__(");
                PrintExpr(f, var);
                fprintf(f, ", ");
                PrintExpr(f, a);
                fprintf(f, ", ");
                PrintExpr(f, b);
                fprintf(f, ")");
            } else {
                PrintBoolExpr(f, AstOperator(T_EQ, var, ast->left));
            }
        }
        needor = 1;
        ast = ast->right;
    }
}

static int
PrintCaseItem(FILE *f, AST *var, AST *ast, int indent)
{
    int sawreturn;

    fprintf(f, "(");
    PrintCaseExprList(f, var, ast->left);
    fprintf(f, ") {\n");
    sawreturn = PrintStatementList(f, ast->right, indent);
    fprintf(f, "%*c}", indent, ' ');
    return sawreturn;
}

static int
PrintCaseStmt(FILE *f, AST *expr, AST *ast, int indent)
{
    int sawreturn = 1;
    int items = 0;
    int first = 1;
    AST *var;

    if (expr->kind == AST_IDENTIFIER) {
        var = expr;
    } else {
        var = AstTempVariable(NULL);
        fprintf(f, "%*cint32_t %s = ", indent, ' ', var->d.string);
        PrintExpr(f, expr);
        fprintf(f, ";\n");
    }
    while (ast) {
        if (ast->kind != AST_LISTHOLDER) {
            ERROR(ast, "Internal error in case list");
            return 0;
        }
        if (first) {
            fprintf(f, "%*cif ", indent, ' ');
            first = 0;
        } else {
            fprintf(f, " else if ");
        }
        sawreturn = PrintCaseItem(f, var, ast->left, indent) && sawreturn;
        ast = ast->right;
        items++;
    }
    fprintf(f, "\n");
    return (items > 0) && sawreturn;
}

/*
 * print a counting repeat loop
 */
static int
PrintCountRepeat(FILE *f, AST *ast, int indent)
{
    AST *fromval, *toval;
    AST *stepval;
    AST *limit;
    AST *step;
    AST *loop_le_limit, *loop_ge_limit;
    AST *loopvar = NULL;
    AST *loopleft, *loopright;
    AST *stepstmt;
    int sawreturn = 0;
    int negstep = 0;
    int needsteptest = 1;
    int deltaknown = 0;
    int32_t delta;
    int needindent;
    int useForLoop = 0;

    if (ast->left) {
        if (ast->left->kind == AST_IDENTIFIER) {
            loopvar = ast->left;
        } else if (ast->left->kind == AST_RESULT) {
            loopvar = ast->left;
        } else {
            ERROR(ast, "Need a variable name for the loop");
            return 0;
        }
    }
    ast = ast->right;
    if (ast->kind != AST_FROM) {
        ERROR(ast, "expected FROM");
        return 0;
    }
    fromval = ast->left;
    ast = ast->right;
    if (ast->kind != AST_TO) {
        ERROR(ast, "expected TO");
        return 0;
    }
    toval = ast->left;
    ast = ast->right;
    if (ast->kind != AST_STEP) {
        ERROR(ast, "expected STEP");
        return 0;
    }
    if (ast->left) {
        stepval = ast->left;
    } else {
        stepval = AstInteger(1);
    }

    /* for fixed counts (like "REPEAT expr") we get a NULL value
       for toval; this signals that we should be counting
       down from expr to 1
    */
    if (fromval == NULL) {
        needsteptest = 0;
        negstep = 0;
        fromval = AstInteger(1);
        useForLoop= 1;
    } else if (IsConstExpr(fromval) && IsConstExpr(toval)) {
        int32_t fromi, toi;

        fromi = EvalConstExpr(fromval);
        toi = EvalConstExpr(toval);
        needsteptest = 0;
        negstep = (fromi > toi);
    }

    needindent = !loopvar || !IsConstExpr(toval) || !(IsConstExpr(stepval) && !needsteptest);

    if (needindent) {
        fprintf(f, "%*c{\n", indent, ' ');
        indent += 2;
    }

    /* set the loop variable */
    if (!loopvar) {
        loopvar = AstTempVariable("_idx_");
        fprintf(f, "%*cint32_t %s;\n", indent, ' ', loopvar->d.string);
    }

    /* set the limit variable */
    if (IsConstExpr(toval)) {
        limit = AstInteger(EvalConstExpr(toval));
    } else {
        limit = AstTempVariable("_limit_");
        fprintf(f, "%*cint32_t ", indent, ' ');
        PrintExpr(f, limit);
        fprintf(f, " = ");
        PrintExpr(f, toval);
        fprintf(f, ";\n");
    }
    /* set the step variable */
    if (IsConstExpr(stepval) && !needsteptest) {
        delta = EvalConstExpr(stepval);
        if (negstep) delta = -delta;
        step = AstInteger(delta);
        deltaknown = 1;
        if (IsConstExpr(toval) && IsConstExpr(fromval))
            useForLoop = 1;
    } else {
        if (negstep) stepval = AstOperator(T_NEGATE, NULL, stepval);
        step = AstTempVariable("_step_");
        fprintf(f, "%*cint32_t ", indent, ' ');
        PrintExpr(f, step);
        fprintf(f, " = ");
        PrintExpr(f, stepval);
        fprintf(f, ";\n");
    }

    stepstmt = AstAssign('+', loopvar, step);

    if (!useForLoop) {
        /* set the loop variable */
        fprintf(f, "%*c", indent, ' ');
        PrintExpr(f, loopvar);
        fprintf(f, " = ");
        PrintExpr(f, fromval);
        fprintf(f, ";\n");
    }
    /* want to do:
     * if (loopvar > limit) step = -step;
     * do {
     *   body
     * } while ( (step > 0 && loopvar <= limit) || (step < 0 && loopvar >= limit));
     */
    loop_ge_limit = AstOperator(T_GE, loopvar, limit);
    loop_le_limit = AstOperator(T_LE, loopvar, limit);
    if (needsteptest) {
        fprintf(f, "%*cif (", indent, ' ');
        PrintBoolExpr(f, loop_ge_limit);
        fprintf(f, ") ");
        PrintExpr(f, step);
        fprintf(f, " = -");
        PrintExpr(f, step);
        fprintf(f, ";\n");
    }

    if (deltaknown) {
        if (delta > 0) {
            loopleft = loop_le_limit;
            loopright = AstInteger(0);
        } else if (delta < 0) {
            loopleft = AstInteger(0);
            loopright = loop_ge_limit;
        } else {
            loopleft = loopright = AstInteger(0);
        }
    } else {
        loopleft = AstOperator(T_AND, AstOperator('>', step, AstInteger(0)), loop_le_limit);
        loopright = AstOperator(T_AND, AstOperator('<', step, AstInteger(0)), loop_ge_limit);
    }

    if (useForLoop) {

        fprintf(f, "%*cfor(", indent, ' ');
        /* set the loop variable */
        PrintExpr(f, loopvar);
        fprintf(f, " = ");
        PrintExpr(f, fromval);
        fprintf(f, "; ");
        if (IsConstExpr(loopleft)) {
            PrintBoolExpr(f, loopright);
        } else if (IsConstExpr(loopright)) {
            PrintBoolExpr(f, loopleft);
        } else {
            PrintBoolExpr(f, AstOperator(T_OR, loopleft, loopright));
        }
        fprintf(f, "; ");
        if (stepstmt->kind == AST_ASSIGN)
            PrintAssign(f, stepstmt->left, stepstmt->right);
        else
            PrintExpr(f, stepstmt);
        fprintf(f, ") {\n");
        sawreturn = PrintStatementList(f, ast->right, indent+2);
        fprintf(f, "%*c}\n", indent, ' ');
    } else {
        /* use a do/while loop */



        fprintf(f, "%*cdo {\n", indent, ' ');
        sawreturn = PrintStatementList(f, ast->right, indent+2);
        PrintStatement(f, stepstmt, indent+2);
        fprintf(f, "%*c} while (", indent, ' ');
        if (IsConstExpr(loopleft)) {
            PrintBoolExpr(f, loopright);
        } else if (IsConstExpr(loopright)) {
            PrintBoolExpr(f, loopleft);
        } else {
            PrintBoolExpr(f, AstOperator(T_OR, loopleft, loopright));
        }
        fprintf(f, ");\n");
    }
    if (needindent) {
        indent -= 2;
        fprintf(f, "%*c}\n", indent, ' ');
    }
    return sawreturn;
}

/*
 * print declarations for optimizer
 */
static void
PrintOptimizeDecl(FILE *f, AST *ast, int indent)
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
            fprintf(f, "};\n");
        } else {
            ERROR(ast, "internal error in expression re-arranging");
            return;
        }
    }
}

/*
 * returns 1 if a return statement was seen
 */
static int
PrintStatement(FILE *f, AST *ast, int indent)
{
    int sawreturn = 0;
    AST *lhsast = NULL;
    AST *comment = NULL;

    if (!ast) return 0;

    switch (ast->kind) {
    case AST_COMMENTEDNODE:
        PrintIndentedComment(f, ast->right, indent);
        sawreturn = PrintStatement(f, ast->left, indent);
        break;
    case AST_RETURN:
        fprintf(f, "%*creturn ", indent, ' ');
        if (ast->left) {
            PrintExpr(f, ast->left);
        } else {
            PrintExpr(f, curfunc->resultexpr);
        }
        fprintf(f, ";\n");
        sawreturn = 1;
        break;
    case AST_ABORT:
        fprintf(f, "%*cif (!abortChain__) abort();\n", indent, ' ');
        fprintf(f, "%*cabortChain__->val =  ", indent, ' ');
        if (ast->left) {
            PrintExpr(f, ast->left);
        } else {
            PrintExpr(f, curfunc->resultexpr);
        }
        fprintf(f, ";\n");
        fprintf(f, "%*clongjmp(abortChain__->jmp, 1);\n", indent, ' ');
        break;
    case AST_YIELD:
        fprintf(f, "%*cYield__();\n", indent, ' ');
        break;
    case AST_QUIT:
        fprintf(f, "%*cbreak;\n", indent, ' ');
        break;
    case AST_NEXT:
        fprintf(f, "%*ccontinue;\n", indent, ' ');
        break;
    case AST_IF:
        fprintf(f, "%*cif (", indent, ' ');
        PrintBoolExpr(f, ast->left);
        fprintf(f, ") {\n");
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
            return 0;
        }
        sawreturn = PrintStatementList(f, ast->left, indent+2);
        if (ast->right) {
            fprintf(f, "%*c} else {\n", indent, ' ');
            if (comment) {
                PrintIndentedComment(f, comment, indent+2);
                comment = NULL;
            }
            sawreturn = PrintStatementList(f, ast->right, indent+2) && sawreturn;
        } else {
            sawreturn = 0;
        }
        fprintf(f, "%*c}\n", indent, ' ');
        break;
    case AST_WHILE:
        fprintf(f, "%*cwhile (", indent, ' ');
        PrintBoolExpr(f, ast->left);
        fprintf(f, ") {\n");
        sawreturn = PrintStatementList(f, ast->right, indent+2);
        fprintf(f, "%*c}\n", indent, ' ');
        break;
    case AST_DOWHILE:
        fprintf(f, "%*cdo {\n", indent, ' ');
        sawreturn = PrintStatementList(f, ast->right, indent+2);
        fprintf(f, "%*c} while (", indent, ' ');
        PrintBoolExpr(f, ast->left);
        fprintf(f, ");\n");
        break;
    case AST_COUNTREPEAT:
        sawreturn = PrintCountRepeat(f, ast, indent);
        break;
    case AST_STMTLIST:
        sawreturn = PrintStatementList(f, ast, indent+2);
        break;
    case AST_CASE:
        sawreturn = PrintCaseStmt(f, ast->left, ast->right, indent);
        break;
    case AST_POSTEFFECT:
        fprintf(f, "%*c", indent, ' ');
        PrintPostfix(f, ast, 1);
        fprintf(f, ";\n");
        break;
    case AST_OPERATOR:
        switch (ast->d.ival) {
        case T_NEGATE:
        case T_ABS:
        case T_SQRT:
        case T_BIT_NOT:
        case T_DECODE:
        case T_ENCODE:
            lhsast = DupAST(ast->right);
            ast = AstAssign(T_ASSIGN, lhsast, ast);
            break;
        }
        /* fall through */
    default:
    case AST_ASSIGN:
        fprintf(f, "%*c", indent, ' ');
        if (ast->kind == AST_ASSIGN) {
            PrintAssign(f, ast->left, ast->right);
        } else {
            PrintExpr(f, ast);
        }
        fprintf(f, ";\n");
        break;
    }
    return sawreturn;
}

static int
PrintFunctionStmts(FILE *f, Function *func)
{
    return PrintStatementList(f, func->body, 2);
}


void
PrintFunctionBodies(FILE *f, ParserState *parse)
{
    Function *pf;
    int sawreturn;

    for (pf = parse->functions; pf; pf = pf->next) {
        AST *optdecl;
        optdecl = Optimize(pf->body, pf);
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
        if (gl_ccode) {
            if (!pf->is_public) {
                fprintf(f, "static ");
            }
            fprintf(f, "int32_t %s_%s(", parse->classname, pf->name);
            fprintf(f, "%s *self", parse->classname);
            if (pf->params) {
                fprintf(f, ", ");
                PrintParameterList(f, pf->params);
            }
        } else {
            fprintf(f, "int32_t %s::%s(", parse->classname, pf->name);
            PrintParameterList(f, pf->params);
        }
        fprintf(f, ")\n{\n");
        if (!pf->result_used) {
            pf->resultexpr = AstInteger(0);
        }
        PrintFunctionVariables(f, pf);
        if (optdecl) {
            PrintOptimizeDecl(f, optdecl, 2);
            fprintf(f, "\n");
        }
        sawreturn = PrintFunctionStmts(f, pf);

        if (!sawreturn) {
            fprintf(f, "  return ");
            PrintExpr(f, pf->resultexpr);
            fprintf(f, ";\n");
        }
        fprintf(f, "}\n\n");
        curfunc = NULL;
    }
}

/*
 * parse annotation directives
 */
int match(const char *str, const char *pat)
{
    return !strncmp(str, pat, strlen(pat));
}

static void
ParseDirectives(const char *str)
{
    if (match(str, "nospin"))
        gl_nospin = 1;
    else if (match(str, "ccode"))
        gl_ccode = 1;
}

/*
 * an annotation just looks like a function with no name or body
 */
void
DeclareAnnotation(AST *anno)
{
    Function *f;
    const char *str;

    /* check the annotation string; some of them are special */
    str = anno->d.string;
    if (*str == '!') {
        /* directives, not code */
        str += 1;
        ParseDirectives(str);
    } else {
        f = NewFunction();
        f->annotations = anno;
    }
}
