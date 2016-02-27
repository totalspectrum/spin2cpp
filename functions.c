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

Function *curfunc;
static int visitPass = 1;

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
EnterVars(int kind, SymbolTable *stab, void *symval, AST *varlist, int count)
{
    AST *lower;
    AST *ast;
    Symbol *sym;
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
    case AST_ABSADDROF:
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
        sym = FindSymbol(&fdef->localsyms,  body->d.string);
        if (!sym) {
            sym = LookupSymbol(body->d.string);
        }
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
        break;
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
    int is_public;
    int localcount;
    
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
    fdef->decl = funcdef;
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
    fdef->rettype = ast_type_generic;

    vars = funcdef->right;
    if (vars->kind != AST_FUNCVARS) {
        ERROR(vars, "Internal error: bad variable declaration");
    }

    /* enter the variables into the local symbol table */
    fdef->params = vars->left;
    fdef->locals = vars->right;
    fdef->numparams = EnterVars(SYM_PARAMETER, &fdef->localsyms, ast_type_long, fdef->params, 0);
    localcount = EnterVars(SYM_LOCALVAR, &fdef->localsyms, ast_type_long, fdef->locals, 0);

    AddSymbol(&fdef->localsyms, resultname, SYM_RESULT, ast_type_long);

    fdef->body = body;

    /* define the function itself */
    AddSymbol(&current->objsyms, fdef->name, SYM_FUNCTION, fdef);

    /* check for special conditions */
    ScanFunctionBody(fdef, body, NULL);

    /* if we put the locals into an array, record the size of that array */
    if (fdef->localarray) {
        fdef->localarray_len += localcount;
    } 
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
 * Normalization of function and expression structures
 *
 * there are a number of simple optimizations we can perform on a function
 * (1) Convert lookup/lookdown into constant array references
 * (2) Eliminate unused result variables
 *
 * Called recursively; the top level call has ast = func->body
 * Returns an AST that should be printed before the function body, e.g.
 * to declare temporary arrays.
 */
static AST *
NormalizeFunc(AST *ast, Function *func)
{
    AST *ldecl;
    AST *rdecl;

    if (!ast)
        return NULL;

    switch (ast->kind) {
    case AST_RETURN:
        if (ast->left) {
            return NormalizeFunc(ast->left, func);
        }
        return NULL;
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
        ldecl = NormalizeFunc(ast->left, func);
        rdecl = NormalizeFunc(ast->right, func);
        return AddToList(ldecl, rdecl);
    }
}

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
                    sym->count += offset;
                    n = typeSize((AST *)sym->val);
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
 * add a local variable to a function
 */
void
AddLocalVariable(Function *func, AST *var)
{
    AST *varlist = NewAST(AST_LISTHOLDER, var, NULL);
    EnterVars(SYM_LOCALVAR, &func->localsyms, ast_type_long, varlist, func->localarray_len);
    func->locals = AddToList(func->locals, NewAST(AST_LISTHOLDER, var, NULL));
    if (func->localarray) {
        func->localarray_len++;
    }
}

/*
 * print a counting repeat loop
 */
AST *
TransformCountRepeat(AST *ast)
{
    AST *origast = ast;
    AST *fromval, *toval;
    AST *stepval;
    AST *limit;
    AST *step;
    AST *loop_le_limit, *loop_ge_limit;
    AST *loopvar = NULL;
    AST *loopleft, *loopright;
    AST *initstmt;
    AST *condtest;
    AST *stepstmt;
    AST *forast;
    AST *body;
    AST *initvar = NULL;
    
    int negstep = 0;
    int needsteptest = 1;
    int deltaknown = 0;
    int32_t delta = 0;
    int useLt = 0;
    int saveLineNum = 0;
    
    if (ast->left) {
        if (ast->left->kind == AST_IDENTIFIER) {
            loopvar = ast->left;
        } else if (ast->left->kind == AST_RESULT) {
            loopvar = ast->left;
        } else {
            ERROR(ast, "Need a variable name for the loop");
            return origast;
        }
    }
    ast = ast->right;
    if (ast->kind != AST_FROM) {
        ERROR(ast, "expected FROM");
        return origast;
    }
    fromval = ast->left;
    ast = ast->right;
    if (ast->kind != AST_TO) {
        ERROR(ast, "expected TO");
        return origast;
    }
    toval = ast->left;
    ast = ast->right;
    if (ast->kind != AST_STEP) {
        ERROR(ast, "expected STEP");
        return origast;
    }
    if (ast->left) {
        stepval = ast->left;
    } else {
        stepval = AstInteger(1);
    }
    body = ast->right;
    
    if (current) {
        // temporarily pretend to be at a different place in the file,
        // so that the NewAST calls get the right line number
        saveLineNum = current->L.lineCounter;
        current->L.lineCounter = toval ? toval->line : origast->line;
    }
    /* for fixed counts (like "REPEAT expr") we get a NULL value
       for fromval; this signals that we should be counting
       from 0 to toval - 1 (in C) or from toval down to 1 (in asm)
    */
    if (fromval == NULL) {
        needsteptest = 0;
        if (gl_outcode == OUTCODE_C || gl_outcode == OUTCODE_CPP) {
            useLt = 1;
            fromval = AstInteger(0);
            negstep = 0;
        } else {
            fromval = toval;
            toval = AstInteger(1);
            negstep = 1;
        }
    } else if (IsConstExpr(fromval) && IsConstExpr(toval)) {
        int32_t fromi, toi;

        fromi = EvalConstExpr(fromval);
        toi = EvalConstExpr(toval);
        needsteptest = 0;
        negstep = (fromi > toi);
    }

    /* set the loop variable */
    if (!loopvar) {
        loopvar = AstTempVariable("_idx_");
        AddLocalVariable(curfunc, loopvar);
    }

    if (!IsConstExpr(fromval)) {
        initvar = AstTempVariable("_start_");
        AddLocalVariable(curfunc, initvar);
        initstmt = AstAssign(T_ASSIGN, loopvar, AstAssign(T_ASSIGN, initvar, fromval));
    } else {
        initstmt = AstAssign(T_ASSIGN, loopvar, fromval);
        initvar = fromval;
    }
    /* set the limit variable */
    if (IsConstExpr(toval)) {
        if (gl_expand_constants) {
            limit = AstInteger(EvalConstExpr(toval));
        } else {
            limit = toval;
        }
    } else {
        limit = AstTempVariable("_limit_");
        AddLocalVariable(curfunc, limit);
        initstmt = NewAST(AST_SEQUENCE, initstmt, AstAssign(T_ASSIGN, limit, toval));
    }
    /* set the step variable */
    if (IsConstExpr(stepval) && !needsteptest) {
        delta = EvalConstExpr(stepval);
        if (negstep) delta = -delta;
        step = AstInteger(delta);
        deltaknown = 1;
    } else {
        if (negstep) stepval = AstOperator(T_NEGATE, NULL, stepval);
        step = AstTempVariable("_step_");
        AddLocalVariable(curfunc, step);
        initstmt = NewAST(AST_SEQUENCE, initstmt, AstAssign(T_ASSIGN, step, stepval));
    }

    if (deltaknown && delta == 1) {
        stepstmt = AstOperator(T_INCREMENT, loopvar, NULL);
    } else if (deltaknown && delta == -1) {
        stepstmt = AstOperator(T_INCREMENT, NULL, loopvar);
    } else {
        stepstmt = AstAssign('+', loopvar, step);
    }
    
    /* want to do:
     * if (loopvar > limit) step = -step;
     * do {
     *   body
     * } while ( (step > 0 && loopvar <= limit) || (step < 0 && loopvar >= limit));
     */

    loop_ge_limit = AstOperator(T_GE, loopvar, limit);

    /* try to make things a bit more idiomatic; if the limit is N - 1,
       change the ge_limit to actually be "< N" rather than "<= N - 1"
    */
    if (!useLt && limit->kind == AST_OPERATOR && limit->d.ival == '-'
        && limit->right->kind == AST_INTEGER
        && limit->right->d.ival == 1)
    {
        loop_le_limit = AstOperator('<', loopvar, limit->left);
    } else if (useLt) {
        loop_le_limit = AstOperator('<', loopvar, limit);
    } else {
        loop_le_limit = AstOperator(T_LE, loopvar, limit);
    }
    if (needsteptest) {
        AST *fixstep;
        fixstep = NewAST( AST_CONDRESULT, loop_ge_limit,
                          NewAST(AST_THENELSE,
                                 AstOperator(T_NEGATE, NULL, step),
                                 step) );
        initstmt = NewAST(AST_SEQUENCE, initstmt,
                          AstAssign(T_ASSIGN, step, fixstep));
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

    if (IsConstExpr(loopleft)) {
        condtest = loopright;
    } else if (IsConstExpr(loopright)) {
        condtest = loopleft;
    } else {
        // condtest = AstOperator(T_OR, loopleft, loopright);
        // use the AST_BETWEEN operator for better code
        condtest = NewAST(AST_ISBETWEEN, loopvar, NewAST(AST_RANGE, initvar, limit));
        /* the loop has to execute at least once */
        if (gl_outcode == OUTCODE_C || gl_outcode == OUTCODE_CPP) {
            condtest = AstOperator(T_OR, condtest, AstOperator(T_EQ, loopvar, fromval));
        }
    }

    // optimize counting down to 1; x != 0 is much faster than x >= 1
    if (deltaknown && delta == -1) {
        if (condtest->kind == AST_OPERATOR && condtest->d.ival == T_GE
            && IsConstExpr(condtest->right)
            && 1 == EvalConstExpr(condtest->right))
        {
            AST *lhs = condtest->left;
            condtest = AstOperator(T_NE, lhs, AstInteger(0));
        }
    }
    stepstmt = NewAST(AST_STEP, stepstmt, body);
    condtest = NewAST(AST_TO, condtest, stepstmt);
    forast = NewAST(AST_FORATLEASTONCE, initstmt, condtest);
    if (current) {
        current->L.lineCounter = saveLineNum;
    }
    forast->line = origast->line;
    return forast;
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
PrintFunctionBodies(FILE *f, ParserState *parse)
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
    else if (match(str, "ccode")) {
        if (gl_outcode == OUTCODE_CPP)
            gl_outcode = OUTCODE_C;
    }
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

/*
 * type inference
 */

/* check for static member functions, i.e. ones that do not
 * use any variables in the object, and hence can be called
 * without the object itself as an implicit parameter
 */
static void
CheckForStatic(Function *fdef, AST *body)
{
    Symbol *sym;
    if (!body || !fdef->is_static) {
        return;
    }
    switch(body->kind) {
    case AST_IDENTIFIER:
        sym = FindSymbol(&fdef->localsyms, body->d.string);
        if (!sym) {
            sym = LookupSymbol(body->d.string);
        }
        if (sym) {
            if (sym->type == SYM_VARIABLE || sym->type == SYM_OBJECT) {
                fdef->is_static = 0;
                return;
            }
            if (sym->type == SYM_FUNCTION) {
                Function *func = (Function *)sym->val;
                if (func) {
                    fdef->is_static = fdef->is_static && func->is_static;
                } else {
                    fdef->is_static = 0;
                }
            }
        } else {
            // assume this is an as-yet-undefined member
            fdef->is_static = 0;
        }
        break;
    default:
        CheckForStatic(fdef, body->left);
        CheckForStatic(fdef, body->right);
    }
}

/*
 * Check for function return type
 * This returns 1 if we see a return statement, 0 if not
 */
static int CheckRetStatement(Function *func, AST *ast);

static int
CheckRetStatementList(Function *func, AST *ast)
{
    int sawreturn = 0;
    while (ast) {
        if (ast->kind != AST_STMTLIST) {
            ERROR(ast, "Internal error: expected statement list, got %d",
                  ast->kind);
            return 0;
        }
        sawreturn |= CheckRetStatement(func, ast->left);
        ast = ast->right;
    }
    return sawreturn;
}

static bool
IsResultVar(Function *func, AST *lhs)
{
    if (lhs->kind == AST_RESULT) {
        return true;
    }
    if (lhs->kind == AST_IDENTIFIER) {
        return AstMatch(lhs, func->resultexpr);
    }
    return false;
}

static int
CheckRetStatement(Function *func, AST *ast)
{
    int sawreturn = 0;
    AST *lhs, *rhs;
    
    if (!ast) return 0;
    switch (ast->kind) {
    case AST_COMMENTEDNODE:
        sawreturn = CheckRetStatement(func, ast->left);
        break;
    case AST_RETURN:
        if (ast->left) {
            SetFunctionType(func, ExprType(ast->left));
        }
        sawreturn = 1;
        break;
    case AST_ABORT:
        if (ast->left) {
            (void)CheckRetStatement(func, ast->left);
            SetFunctionType(func, ExprType(ast->left));
        }
        break;
    case AST_IF:
        ast = ast->right;
        if (ast->kind == AST_COMMENTEDNODE)
            ast = ast->left;
        sawreturn = CheckRetStatementList(func, ast->left);
        sawreturn = CheckRetStatementList(func, ast->right) && sawreturn;
        break;
    case AST_WHILE:
    case AST_DOWHILE:
        sawreturn = CheckRetStatementList(func, ast->right);
        break;
    case AST_COUNTREPEAT:
        lhs = ast->left; // count variable
        if (lhs) {
            if (IsResultVar(func, lhs)) {
                SetFunctionType(func, ast_type_long);
            }
        }
        ast = ast->right; // from value
        ast = ast->right; // to value
        ast = ast->right; // step value
        ast = ast->right; // body
        sawreturn = CheckRetStatementList(func, ast);
        break;
    case AST_STMTLIST:
        sawreturn = CheckRetStatementList(func, ast);
        break;
    case AST_ASSIGN:
        lhs = ast->left;
        rhs = ast->right;
        if (IsResultVar(func, lhs)) {
            SetFunctionType(func, ExprType(rhs));
        }
        sawreturn = 0;
        break;
    default:
        sawreturn = 0;
        break;
    }
    return sawreturn;
}

/*
 * check function calls for correct number of arguments
 */
void
CheckFunctionCalls(AST *ast)
{
    int expectArgs;
    int gotArgs;
    Symbol *sym;
    const char *fname = "function";
    
    if (!ast) {
        return;
    }
    if (ast->kind == AST_FUNCCALL) {
        AST *a;
        sym = FindFuncSymbol(ast, NULL, NULL);
        expectArgs = 0;
        if (sym) {
            fname = sym->name;
            if (sym->type == SYM_BUILTIN) {
                Builtin *b = (Builtin *)sym->val;
                expectArgs = b->numparameters;
            } else if (sym->type == SYM_FUNCTION) {
                Function *f = (Function *)sym->val;
                expectArgs = f->numparams;
            } else {
                ERROR(ast, "Unexpected function type");
                return;
            }
        }
        gotArgs = 0;
        for (a = ast->right; a; a = a->right) {
            gotArgs++;
        }
        if (gotArgs != expectArgs) {
            ERROR(ast, "Bad number of parameters in call to %s: expected %d found %d", fname, expectArgs, gotArgs);
        }
    }
    CheckFunctionCalls(ast->left);
    CheckFunctionCalls(ast->right);
}

/*
 * do basic processing of functions
 */
void
ProcessFuncs(ParserState *P)
{
    Function *pf;
    int sawreturn = 0;

    current = P;
    for (pf = P->functions; pf; pf = pf->next) {
        CheckRecursive(pf);  /* check for recursive functions */
        pf->extradecl = NormalizeFunc(pf->body, pf);

        CheckFunctionCalls(pf->body);
        
        /* check for void functions */
        pf->rettype = NULL;
        sawreturn = CheckRetStatementList(pf, pf->body);
        if (pf->rettype == NULL && pf->result_used) {
            /* there really is a return type */
            pf->rettype = ast_type_generic;
        }
        if (pf->rettype == NULL) {
            pf->rettype = ast_type_void;
            pf->resultexpr = NULL;
        } else {
            if (!pf->result_used) {
                pf->resultexpr = AstInteger(0);
                pf->result_used = 1;
            }
            if (!sawreturn) {
                AST *retstmt;

                retstmt = NewAST(AST_STMTLIST, NewAST(AST_RETURN, pf->resultexpr, NULL), NULL);
                pf->body = AddToList(pf->body, retstmt);
            }
        }
    }
}

/*
 * main entry for type checking
 */
int
InferTypes(ParserState *P)
{
    Function *pf;
    int changes = 0;
    
    /* scan for static definitions */
    current = P;
    for (pf = P->functions; pf; pf = pf->next) {
        if (pf->is_static) {
            continue;
        }
        pf->is_static = 1;
        CheckForStatic(pf, pf->body);
        if (pf->is_static) {
            changes++;
        }
    }
    return changes;
}

static void
MarkUsedBody(AST *body)
{
    Symbol *sym, *objsym;
    AST *objref;
    
    if (!body) return;
    switch(body->kind) {
    case AST_IDENTIFIER:
        sym = LookupSymbol(body->d.string);
        if (sym && sym->type == SYM_FUNCTION) {
            Function *func = (Function *)sym->val;
            MarkUsed(func);
        }
        break;
    case AST_METHODREF:
        objref = body->left;
        objsym = LookupAstSymbol(objref, "object reference");
        if (!objsym) return;
        if (objsym->type != SYM_OBJECT) {
            ERROR(body, "%s is not an object", objsym->name);
            return;
        }
        sym = LookupObjSymbol(body, objsym, body->right->d.string);
        if (!sym || sym->type != SYM_FUNCTION) {
            return;
        }
        MarkUsed((Function *)sym->val);
        break;
    default:
        MarkUsedBody(body->left);
        MarkUsedBody(body->right);
        break;
    }
}

void
MarkUsed(Function *f)
{
    ParserState *oldcurrent;
    if (!f || f->is_used) {
        return;
    }
    f->is_used = 1;
    oldcurrent = current;
    current = f->parse;
    MarkUsedBody(f->body);
    current = oldcurrent;
}

void
SetFunctionType(Function *f, AST *typ)
{
    f->rettype = typ;
}

/*
 * check to see if function ref may be called from AST body
 */
bool
IsCalledFrom(Function *ref, AST *body, int visitRef)
{
    ParserState *oldState;
    Symbol *sym;
    Function *func;
    bool result;
    
    if (!body) return false;
    switch(body->kind) {
    case AST_FUNCCALL:
        sym = FindFuncSymbol(body, NULL, NULL);
        if (!sym) return false;
        if (sym->type != SYM_FUNCTION) return false;
        func = (Function *)sym->val;
        if (ref == func) return true;
        if (func->visitFlag == visitRef) {
            // we've been here before
            return false;
        }
        func->visitFlag = visitRef;
        oldState = current;
        current = func->parse;
        result = IsCalledFrom(ref, func->body, visitRef);
        current = oldState;
        return result;
    default:
        return IsCalledFrom(ref, body->left, visitRef)
            || IsCalledFrom(ref, body->right, visitRef);
        break;
    }
    return false;
}

void
CheckRecursive(Function *f)
{
    visitPass++;
    f->is_recursive = IsCalledFrom(f, f->body, visitPass);
}


/*
 * SpinTransform
 * transform AST to reflect some oddities of the Spin language:
 * (1) It's legal to call a void function, just substitute 0 for the result
 * (2) Certain operators used at top level are changed into assignments
 * (3) Validate parameters to some builtins
 * (4) Turn AST_COUNTREPEAT into AST_FOR
 */
/* if level is 0, we are inside an expression
 * level == 1 at top level
 * level == 2 inside a coginit
 */
static void
doSpinTransform(AST **astptr, int level)
{
    AST *ast = *astptr;
    Symbol *sym;
    Function *func;
    
    while (ast && ast->kind == AST_COMMENTEDNODE) {
        astptr = &ast->left;
        ast = *astptr;
    }
    if (!ast) return;
    switch (ast->kind) {
    case AST_THENELSE:
        doSpinTransform(&ast->left, level);
        doSpinTransform(&ast->right, level);
        break;
    case AST_RETURN:
    case AST_ABORT:
        doSpinTransform(&ast->left, 0);
        break;
    case AST_IF:
    case AST_WHILE:
    case AST_DOWHILE:
        doSpinTransform(&ast->left, 0);
        doSpinTransform(&ast->right, level);
        break;
    case AST_COUNTREPEAT:
        ast = ast->right; // from value
        doSpinTransform(&ast->left, 0);
        ast = ast->right; // to value
        doSpinTransform(&ast->left, 0);
        ast = ast->right; // step value
        doSpinTransform(&ast->left, 0);
        doSpinTransform(&ast->right, level);

        /* now fix it up */
        *astptr = TransformCountRepeat(*astptr);
        break;
    case AST_STMTLIST:
        doSpinTransform(&ast->left, level);
        doSpinTransform(&ast->right, level);
        break;
    case AST_CASE:
    {
        AST *list = ast->right;
        doSpinTransform(&ast->left, 0);
        while (list) {
            doSpinTransform(&list->left->left, 0);
            doSpinTransform(&list->left->right, level);
            list = list->right;
        }
        break;
    }
    case AST_COGINIT:
        if (0 != (func = IsSpinCoginit(ast))) {
            current->needsCoginit = 1;
            if (!func->is_static) {
                func->force_static = 1;
                func->is_static = 1;
            }
        }
        doSpinTransform(&ast->right, 2);
        break;
    case AST_FUNCCALL:
        if (level == 0) {
            /* check for void functions here */
            sym = FindFuncSymbol(ast, NULL, NULL);
            if (sym && sym->type == SYM_FUNCTION) {
                Function *f = (Function *)sym->val;
                if (f->rettype == ast_type_void) {
                    AST *seq = NewAST(AST_SEQUENCE, ast, AstInteger(0));
                    *astptr = seq;
                }
            }
        }
        doSpinTransform(&ast->left, 0);
        doSpinTransform(&ast->right, 0);
        break;
    case AST_WAITPEQ:
    case AST_WAITPNE:
    {
        int n;
        const char *name = (ast->kind == AST_WAITPNE) ? "waitpne" : "waitpeq";
        AST *args = ast->left;
        n = AstListLen(args);
        if (n != 3) {
            ERROR(ast, "Bad number of parameters in call to %s: expected 3 found %d", name, n);
        } else {
            args = args->right->right->left;  // get 3rd parameter
            if (!IsConstExpr(args) || EvalConstExpr(args) != 0) {
                ERROR(args, "Final parameter to %s must be 0", name);
            }
        }
        doSpinTransform(&ast->left, 0);
        doSpinTransform(&ast->right, 0);
        break;
    }
    case AST_WAITVID:
    {
        int n;
        const char *name = "waitvid";
        AST *args = ast->left;
        n = AstListLen(args);
        if (n != 2) {
            ERROR(ast, "Bad number of parameters in call to %s: expected 2 found %d", name, n);
        }
        doSpinTransform(&ast->left, 0);
        doSpinTransform(&ast->right, 0);
        break;
    }
    case AST_POSTEFFECT:
        if (level != 1 && current) {
	    current->needsPosteffect = 1;
	}
        doSpinTransform(&ast->left, 0);
        doSpinTransform(&ast->right, 0);
	break;
    case AST_OPERATOR:
        if (level == 1) {
            AST *lhsast;
            int line = ast->line;
            switch (ast->d.ival) {
            case T_NEGATE:
            case T_ABS:
            case T_SQRT:
            case T_BIT_NOT:
            case T_DECODE:
            case T_ENCODE:
                lhsast = DupAST(ast->right);
                *astptr = ast = AstAssign(T_ASSIGN, lhsast, ast);
                ast->line = lhsast->line = line;
                break;
            }
        } else {
            AST *lhsast;
            switch (ast->d.ival) {
            case T_DECODE:
                lhsast = AstOperator(T_SHL, AstInteger(1), ast->right);
                lhsast->line = ast->line;
                *astptr = ast = lhsast;
                break;
            }
        }
        /* fall through */
    default:
        doSpinTransform(&ast->left, 0);
        doSpinTransform(&ast->right, 0);
        break;
    }
}

void
SpinTransform(ParserState *Q)
{
    Function *func;
    Function *savefunc = curfunc;
    current = Q;
    for (func = Q->functions; func; func = func->next) {
        curfunc = func;
        doSpinTransform(&func->body, 1);
    }
    curfunc = savefunc;
}
