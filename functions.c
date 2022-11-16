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
#include <ctype.h>
#include <inttypes.h>
#include "spinc.h"
#include "preprocess.h"

// marked in MarkUsed(); checks for things like try()/catch() that
// may affect global code generation
int gl_features_used;

/* forward declaration */
static int InferTypesStmt(AST *);
static int InferTypesExpr(AST *expr, AST *expectedType);

/*
 * how many items will this expression put on the stack?
 */
int
NumExprItemsOnStack(AST *expr)
{
    AST *type;
    int siz;
    if (!expr) {
        return 0;
    }
    type = ExprType(expr);
    if (!type) {
        return 1;
    }
    if (type == ast_type_void) {
        return 0;
    }
    if (IsCLang(curfunc->language) && IsArrayType(type)) {
        /* convert a to &a[0] */
        AST *ref = DupAST(expr);
        *expr = *NewAST(AST_ADDROF,
                        NewAST(AST_ARRAYREF,
                               ref, AstInteger(0)), NULL);
        return 1;
    }
    if (TypeGoesOnStack(type)) {
        // we'll pass this as a pointer
        return 1;
    }
    siz = TypeSize(type);
    return ((siz+3) & ~3) / LONG_SIZE;
}

Function *curfunc;
static int visitPass = 1;

static void ReinitFunction(Function *f, int language)
{
    f->module = current;
    memset(&f->localsyms, 0, sizeof(f->localsyms));
    f->localsyms.next = &current->objsyms;
    if (LangCaseInSensitive(language)) {
        f->localsyms.flags = SYMTAB_FLAG_NOCASE;
    }
    f->optimize_flags = gl_optimize_flags;
}
static const char *
FindAnnotation(AST *annotations, const char *key)
{
    const char *ptr;
    int len = strlen(key);
    while (annotations) {
        if (annotations->kind == AST_ANNOTATION) {
            ptr = annotations->d.string;
            while (ptr && *ptr == '(') ptr++;
            while (ptr && *ptr) {
                if (!strncmp(ptr, key, len)) {
                    if (ptr[len] == 0 || !isalpha(ptr[len])) {
                        return ptr+len;
                    }
                }
                while (*ptr && *ptr != ',') ptr++;
                if (*ptr == ',') ptr++;
            }
        }
        annotations = annotations->right;
    }
    return 0;
}

Function *
NewFunction(int language)
{
    Function *f;
    Function *pf;

    f = (Function *)calloc(1, sizeof(*f));
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
    /* and initialize */
    ReinitFunction(f, language);
    return f;
}

static Symbol *
EnterVariable(int kind, SymbolTable *stab, AST *astname, AST *type, unsigned sym_flag)
{
    Symbol *sym;
    const char *name;
    const char *username;

    if (astname->kind == AST_LOCAL_IDENTIFIER) {
        name = astname->left->d.string;
        username = astname->right->d.string;
    } else if (astname->kind == AST_IDENTIFIER) {
        name = username = astname->d.string;
    } else if (astname->kind == AST_VARARGS) {
        name = username = "__varargs";
    } else {
        ERROR(astname, "Internal error, bad identifier");
        return NULL;
    }
    
    sym = AddSymbolPlaced(stab, name, kind, (void *)type, username, astname);
    if (!sym) {
        // ignore duplicate definitions for SYM_TEMPVAR
        if (kind == SYM_TEMPVAR && (!type || type == ast_type_generic)) {
            return sym;
        }
        ERROR(astname, "duplicate definition for %s", username);
    } else {
        sym->flags |= sym_flag;
        sym->module = (void *)current;
        if (current && current != systemModule) {
            if ( (gl_warn_flags & WARN_HIDE_MEMBERS)
                 || ( (gl_warn_flags & WARN_LANG_EXTENSIONS) && current->curLanguage == LANG_SPIN_SPIN2 ) )
            {
                Symbol *sym2;
                switch (kind) {
                case SYM_PARAMETER:
                case SYM_RESULT:
                case SYM_LOCALVAR:
                    if ( (sym2 = FindSymbol(&current->objsyms, username)) != 0) {
                        switch (sym2->kind) {
                        case SYM_WEAK_ALIAS:
                        case SYM_FUNCTION:
                            /* no warning for this */
                            break;
                        default:
                            WARNING(astname, "definition of %s hides a member variable", username);
                            if (sym2->def) {
                                NOTE((AST *)sym2->def, "previous definition of %s is here", username);
                            }
                            break;
                        }
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }
    return sym;
}

static AST *
ArrayDeclType(AST *indices, AST *basetype, AST *baseptr, AST *ident)
{
    AST *arraytype;
    if (!indices) {
        return basetype;
    }
    if (indices->kind == AST_EXPRLIST) {
        basetype = ArrayDeclType(indices->right, basetype, baseptr, ident);
        arraytype = NewAST(AST_ARRAYTYPE, basetype, indices->left);
    } else {
        if (!IsConstExpr(indices)) {
            if (ident) {
                ERROR(ident, "Unable to determine size of array %s", GetUserIdentifierName(ident));
                indices = AstInteger(1);
            }
        }
        arraytype = NewAST(AST_ARRAYTYPE, basetype, indices);
    }
    arraytype->d.ptr = baseptr;

    return arraytype;
}

int
EnterVars(int kind, SymbolTable *stab, AST *defaulttype, AST *varlist, int offset, int isUnion, unsigned sym_flags)
{
    AST *lower;
    AST *ast;
    Symbol *sym;
    AST *actualtype;
    int size;
    int typesize;
    ASTReportInfo saveinfo;
    
    for (lower = varlist; lower; lower = lower->right) {
        if (lower->kind == AST_LISTHOLDER) {
            ast = lower->left;
            AstReportAs(ast, &saveinfo);
            if (ast->kind == AST_DECLARE_VAR) {
                actualtype = ast->left;
                ast = ast->right;
            } else {
                actualtype = defaulttype;
            }
            if (actualtype) {
                typesize = CheckedTypeSize(actualtype);
            } else {
                typesize = 4;
            }
            if (kind == SYM_LOCALVAR || kind == SYM_TEMPVAR || kind == SYM_PARAMETER) {
                // keep things in registers, generally
                if (typesize < 4) typesize = 4;
            }
            
            if (!ast) {
                ast = AstTempIdentifier("_param_");
            }
            switch (ast->kind) {
            case AST_INTTYPE:
            case AST_UNSIGNEDTYPE:
            case AST_ARRAYTYPE:
            case AST_PTRTYPE:
            case AST_FLOATTYPE:
            case AST_VOIDTYPE:
            case AST_GENERICTYPE:
            case AST_MODIFIER_CONST:
            case AST_MODIFIER_VOLATILE:
            case AST_FUNCTYPE:
            case AST_OBJECT:
                // a type with no associated variable
                actualtype = ast;
                ast = AstTempIdentifier("_param_");
                // fall through
            case AST_VARARGS:
            case AST_IDENTIFIER:
            case AST_LOCAL_IDENTIFIER:
                sym = EnterVariable(kind, stab, ast, actualtype, sym_flags);
                if (sym) sym->offset = offset;
                if (ast->kind != AST_VARARGS && !isUnion) {
                    offset += typesize;
                }
                break;
            case AST_ARRAYDECL:
            {
                AST *arraytype;
                AST *indices = ast->right;

                arraytype = ArrayDeclType(indices, actualtype, (AST *)ast->d.ptr, ast->left);

                sym = EnterVariable(kind, stab, ast->left, arraytype, sym_flags);
                if (sym) sym->offset = offset;
                if (!isUnion) {
                    size = TypeSize(arraytype);
                    offset += size;
                }
                break;
            }
            case AST_ANNOTATION:
                /* just ignore it */
                break;
            case AST_ASSIGN:
            {
                AST *id = ast->left;
                while (id && id->kind != AST_IDENTIFIER) {
                    id = id->left;
                }
                if (id) {
                    ERROR(ast, "initialization is not supported for member variable %s", GetIdentifierName(id));
                } else {
                    ERROR(ast, "initialization is not supported for variables of this kind");
                }
                return offset;
            }
            default:
                /* this may be a type with no variable */
                
                ERROR(ast, "Internal error, bad AST value %d", ast->kind);
                break;
            }
        } else {
            ERROR(lower, "Expected list of variables, found %d instead", lower->kind);
            AstReportDone(&saveinfo);
            return offset;
        }
        AstReportDone(&saveinfo);
    }
    return offset;
}

/*
 * declare a function and create a Function structure for it
 */

AST *
DeclareFunction(Module *P, AST *rettype, int is_public, AST *funcdef, AST *body, AST *annotation, AST *comment)
{
    AST *funcblock;
    AST *holder;
    AST *funcdecl;
    AST *retinfoholder;
    
    holder = NewAST(AST_FUNCHOLDER, funcdef, body);
    holder->d.ival = P->curLanguage;
    
    funcblock = NewAST(is_public ? AST_PUBFUNC : AST_PRIFUNC, holder, annotation);
    funcblock->d.ptr = (void *)comment;
    funcblock = NewAST(AST_LISTHOLDER, funcblock, NULL);

    funcdecl = funcdef->left;

    // a bit of a hack here, undone in doDeclareFunction
    retinfoholder = NewAST(AST_RETURN, rettype, funcdecl->right);
    funcdecl->right = retinfoholder;

    P->funcblock = AddToList(P->funcblock, funcblock);
    return funcblock->left;
}

/* like DeclareFunction, but takes a function type */
AST *
DeclareTypedFunction(Module *P, AST *ftype, AST *name, int is_public, AST *fbody, AST *annotations, AST *comment)
{
    AST *funcvars, *funcdef;
    AST *funcdecl = NewAST(AST_FUNCDECL, name, NULL);
    AST *type;

    if (ftype->kind == AST_STATIC) {
        is_public = 0;
        ftype = ftype->left;
    }
    type = ftype->left;
    funcvars = NewAST(AST_FUNCVARS, ftype->right, NULL);
    funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);

    return DeclareFunction(P, type, is_public, funcdef, fbody, annotations, comment);
}

static AST *
TranslateToExprList(AST *listholder)
{
    AST *head, *tail;

    if (!listholder) {
        return NULL;
    }
    tail = TranslateToExprList(listholder->right);
    head = NewAST(AST_EXPRLIST, listholder->left, tail);
    return head;
}

void
MarkSystemFuncUsed(const char *name)
{
    Symbol *sym;
    Function *calledf;
    sym = FindSymbol(&systemModule->objsyms, name);
    if (!sym) {
        //it's OK, not all back ends have all system functions
        //ERROR(NULL, "Internal error could not find %s", name);
        return;
    }
    if (sym->kind == SYM_FUNCTION) {
        calledf = (Function *)sym->val;
        calledf->used_as_ptr = 1;
    }
}

//
// try to infer the return type from a lambda expression
//
static AST *
GuessLambdaReturnType(AST *params, AST *body)
{
    AST *expr;
    AST *r;
    
    if (body->kind == AST_STMTLIST) {
        // look for a return statement
        while (body && body->right != NULL && body->right->kind == AST_STMTLIST) {
            body = body->right;
        }
        if (body->right != NULL)
            return NULL;
        body = body->left;
    }
    if (body->kind == AST_COMMENTEDNODE) {
        body = body->left;
    }
    if (body->kind != AST_RETURN) {
        return NULL;
    }
    expr = body->left;
    if (!expr) {
        return NULL;
    }
    // HACK: temporarily declare the parameters as symbols
    {
        SymbolTable *table = (SymbolTable *)calloc(1, sizeof(SymbolTable));
        table->next = &curfunc->localsyms;
        EnterVars(SYM_PARAMETER, table, NULL, params, 0, 0, 0);
        
        r = ExprTypeRelative(table, expr, NULL);
        free(table);
    }
    return r;
}

//
// undo a local variable delcaration
//
static void
UndoLocalIdentifier(AST *body, AST *ident)
{
    if (!body || body == ident) {
        return;
    }
    if (body->kind == AST_LOCAL_IDENTIFIER
        && !strcmp(body->left->d.string, ident->left->d.string))
    {
        body->kind = AST_IDENTIFIER;
        body->d.string = ident->right->d.string;
        body->left = body->right = NULL;
    }
    else
    {
        UndoLocalIdentifier(body->left, ident);
        UndoLocalIdentifier(body->right, ident);
    }
}

//
// add initializers of the form "ident = expr" to a given sequence "seq"
// the type of ident is "basetype"
//
static AST *
AddInitializers(AST *seq, AST *ident, AST *expr, AST *basetype)
{
    AST *assign;
    AST *sub;
    AST *subtype;
    int i;
    int limit;
    ASTReportInfo saveinfo;

    if (!expr) return seq;
    AstReportAs(expr, &saveinfo);

    if (expr->kind == AST_STRINGPTR && IsArrayType(basetype)) {
        AST *params;
        int srclen, dstlen;
        srclen = AstStringLen(expr);
        dstlen = TypeSize(basetype);
        if (srclen > dstlen) {
            ERROR(expr, "Copying %d bytes to array which has only %d bytes space\n", srclen, dstlen);
        }
        params = NewAST(AST_EXPRLIST,
                       ident,
                       NewAST(AST_EXPRLIST,
                              expr, NULL));
        assign = NewAST(AST_FUNCCALL,
                     AstIdentifier("__builtin_strcpy"),
                     params);
        seq = AddToList(seq, NewAST(AST_SEQUENCE, assign, NULL));
        AstReportDone(&saveinfo);
        return seq;
    } else if (expr->kind == AST_EXPRLIST) {
        expr = FixupInitList(basetype, expr);
        if (IsArrayType(basetype)) {
            if (!IsConstExpr(basetype->right)) {
                ERROR(ident, "Variable length arrays not supported yet");
                AstReportDone(&saveinfo);
                return seq;
            }
            limit = EvalConstExpr(basetype->right);
            subtype = basetype->left;
            for (i = 0; expr && i < limit; i++) {
                sub = NewAST(AST_ARRAYREF, ident, AstInteger(i));
                seq = AddInitializers(seq, sub, expr->left, subtype);
                expr = expr->right;
            }
            while (i < limit) {
                sub = NewAST(AST_ARRAYREF, ident, AstInteger(i));
                seq = AddInitializers(seq, sub, AstInteger(0), basetype->left);
                i++;
            }
            AstReportDone(&saveinfo);
            return seq;
        }
        if (IsClassType(basetype)) {
            Module *P = GetClassPtr(basetype);
            AST *varlist = P ? P->finalvarblock : NULL;
            AST *decl;
            AST *subident;
            AST *curexpr;
            while (varlist && varlist->kind == AST_LISTHOLDER) {
                decl = varlist->left;
                varlist = varlist->right;
                if (decl && decl->kind == AST_DECLARE_VAR) {
                    subtype = decl->left;
                    decl = decl->right;
                    while (decl) {
                        if (decl->kind == AST_LISTHOLDER) {
                            subident = decl->left;
                            decl = decl->right;
                        } else {
                            subident = decl;
                            decl = NULL;
                        }
                        if (expr) {
                            if (expr->kind == AST_EXPRLIST) {
                                curexpr = expr->left;
                                expr = expr->right;
                            } else {
                                curexpr = expr;
                                expr = NULL;
                            }
                        } else {
                            curexpr = AstInteger(0);
                        }
                        sub = NewAST(AST_METHODREF, ident, subident);
                        seq = AddInitializers(seq, sub, curexpr, subtype);
                    }
                }
            }
            AstReportDone(&saveinfo);
            return seq;
        }
    }
    if (IsConstType(basetype)) {
        // cast to avoid warnings
        ident = NewAST(AST_CAST, RemoveTypeModifiers(basetype), ident);
    }
    assign = AstAssign(ident, expr);
    assign->kind = AST_ASSIGN_INIT;
    seq = AddToList(seq, NewAST(AST_SEQUENCE, assign, NULL));
    AstReportDone(&saveinfo);
    return seq;
}

//
// declare some static variables
//

//
// FIXME: someday we should implement scopes other 
// than function scope
//
static void
findLocalsAndDeclare(Function *func, AST *ast)
{
    AST *identlist;
    AST *ident;
    AST *name;
    AST *datatype;
    AST *basetype;
    AST *expr;
    AST *seq = NULL; // sequence for variable initialization
    AST *arrayinfo = NULL;
    int kind;
    bool skipDef;
    
    if (!ast) return;
    kind = ast->kind;
    switch(kind) {
    case AST_DECLARE_ALIAS:
        /* this case may be obsolete now */
        name = ast->left;
        ident = ast->right;
        DeclareAlias(&func->localsyms, name, ident);
        AstNullify(ast);  // delete the declaration so we don't see it again
        return;
    case AST_GLOBALVARS:
        ERROR(ast, "global variable declarations not allowed in functions");
        return;
    case AST_CAST:
        return;
    case AST_DECLARE_VAR:
    case AST_DECLARE_VAR_WEAK:
        identlist = ast->right;
        basetype = ast->left;
        if (basetype && basetype->kind == AST_STATIC) {
            ERROR(ast, "Internal error, saw STATIC");
            return;
        }
        while (identlist) {
            datatype = expr = NULL;
            if (identlist->kind == AST_LISTHOLDER) {
                ident = identlist->left;
                identlist = identlist->right;
            } else {
                ident = identlist;
                identlist = NULL;
            }
            if (ident->kind == AST_ASSIGN) {
                // check for x[] = {a, b, c} type initializers here
                if (IsArrayType(basetype) && basetype->right == NULL) {
                    AST *tmp = FixupInitList(basetype, ident->right);
                    ident->right = tmp;
                    //fixupInitializer(current, ident->right, basetype);
                    if (ident->right->kind == AST_EXPRLIST) {
                        basetype->right = AstInteger(AstListLen(ident->right));
                    } else if (ident->right->kind == AST_STRINGPTR) {
                        basetype->right = AstInteger(AstStringLen(ident->right));
                    }
                }
                // write out an initialization for the variable
                expr = ident->right;
                ident = ident->left;
                seq = AddInitializers(seq, ident, expr, basetype);
                // scan for lambdas?
                findLocalsAndDeclare(func, expr);
            }
            if (ident->kind == AST_ARRAYDECL) {
                name = ident->left;
                arrayinfo = ident;
            } else {
                name = ident;
                arrayinfo = NULL;
            }
            if (kind == AST_DECLARE_VAR_WEAK) {
                Symbol *sym = LookupSymbol(name->d.string);
                skipDef = 0 != sym;
                if (sym && sym->kind == SYM_CONSTANT) {
                    ERROR(ast, "Attempt to redefine constant %s as a variable", sym->user_name);
                    skipDef = false;
                }
            } else {
                skipDef = false;
            }
            if (!skipDef && basetype && basetype->kind == AST_FUNCTYPE
                && IsCLang(func->language))
            {
                // ignore definitions of global functions and such
                // complication: since it's a local variable we've defined
                // an alias for it; we need to undo that here
                skipDef = true;
                if (ident->kind == AST_LOCAL_IDENTIFIER) {
                    UndoLocalIdentifier(func->body, ident);
                }
            }
            if (!skipDef) {
                if (basetype) {
                    datatype = basetype;
                } else {
                    if (expr) {
                        datatype = ExprType(expr);
                    }
                }
                if (!datatype) {
                    datatype = InferTypeFromName(name);
                }
                if (arrayinfo) {
                    datatype = ArrayDeclType(arrayinfo->right, datatype, (AST *)arrayinfo->d.ptr, ident);
                }
                if (datatype && datatype->kind == AST_TYPEDEF) {
                    datatype = datatype->left;
                    EnterVariable(SYM_TYPEDEF, &func->localsyms, name, datatype, 0);
                } else {
                    AddLocalVariable(func, name, datatype, SYM_LOCALVAR);
                }
            }
        }
        // now we can overwrite the original variable declaration
        if (seq) {
            *ast = *seq;
        } else {
            AstNullify(ast);
        }
        return;
    case AST_LAMBDA:
        // we will need a closure for this function
        if (!func->closure) {
            const char *closure_name;
            Module *C;
            Module *Parent = func->module;
            AST *closure_type;
            closure_name = NewTemporaryVariable("__closure__", NULL);
            C = func->closure = NewModule(closure_name, current->curLanguage);
            C->Lptr = current->Lptr;
            closure_type = NewAbstractObject(AstIdentifier(closure_name), NULL, 0);
            closure_type->d.ptr = func->closure;
            AddSymbol(&func->localsyms, closure_name, SYM_CLOSURE, closure_type, NULL);

            C->subclasses = Parent->subclasses;
            C->objsyms.next = &Parent->objsyms;
            Parent->subclasses = C;
            C->superclass = Parent;
            
            // we have to mark the global bytemove and _gc_alloc_managed functions
            // as in used
            MarkSystemFuncUsed("_gc_alloc_managed");
            if (!TraditionalBytecodeOutput()) {
                MarkSystemFuncUsed("bytemove");
            }
        }
        {
            AST *ftype = ast->left;
            AST *fbody = ast->right;
            AST *name = AstIdentifier(NewTemporaryVariable("func_", NULL));
            AST *ptrref;
            
            // check for the return type; we may have to infer this
            if (!ftype->left) {
                ftype->left = GuessLambdaReturnType(ftype->right, fbody);
            }
            // declare the lambda function
            DeclareTypedFunction(func->closure, ftype, name, 1, fbody, NULL, NULL);
            // now turn the lambda into a pointer deref
            ptrref = NewAST(AST_METHODREF, AstIdentifier(func->closure->classname), name);
            // AST_ADDROF can allow a type on the right, which will help
            // ExprType() to avoid having to evaluate the ADDROF on an as
            // yet partially defined function
            ptrref = NewAST(AST_ADDROF, ptrref, ftype);
            *ast = *ptrref;
        }
        // do *not* recurse into sub-parts of the lambda!
        return;
    default:
        break;
    }
    findLocalsAndDeclare(func, ast->left);
    findLocalsAndDeclare(func, ast->right);
}

static void
AddClosureSymbol(Function *f, Module *P, AST *ident)
{
    AST *typ = NULL;
    if (ident->kind == AST_DECLARE_VAR) {
        typ = ident->left;
        ident = ident->right;
    }
    if (ident->kind != AST_IDENTIFIER) {
        ERROR(ident, "internal error in closure def");
        return;
    }
    if (!typ) {
        typ = ExprTypeRelative(&f->localsyms, ident, P);
    }
    MaybeDeclareMemberVar(P, ident, typ, 0, NORMAL_VAR);
}

static void
AdjustParameterTypes(AST *paramlist, int lang)
{
    AST *param;
    AST *type;
    
    while (paramlist) {
        param = paramlist->left;
        if (param->kind == AST_DECLARE_VAR) {
            type = param->left;
            if ( (IsArrayType(type) && (IsCLang(lang) || IsBasicLang(lang)))
		 || (IsClassType(type) && (IsBasicLang(lang) || IsPythonLang(lang)) )
	       )
	    {
                type = BaseType(type);
                type = NewAST(AST_PTRTYPE, type, NULL);
                param->left = type;
            }
            else if (IsClassType(type) && IsCLang(lang) && TypeGoesOnStack(type) ) {
                type = BaseType(type);
                type = NewAST(AST_COPYREFTYPE, type, NULL);
                param->left = type;
            }
        }
        paramlist = paramlist->right;
    }
}

static Function *
doDeclareFunction(AST *funcblock)
{
    AST *holder;
    AST *funcdef;
    AST *body;
    AST *annotation;
    AST *srcname;
    Function *fdef;
    Function *oldcur = curfunc;
    AST *vars;
    AST *src;
    AST *comment;
    AST *defparams;
    AST *retinfo;
    int is_public;
    int language;
    const char *funcname_internal;
    const char *funcname_user;
    AST *oldtype = NULL;
    Symbol *sym;
    int is_cog;
    
    is_public = (funcblock->kind == AST_PUBFUNC);
    holder = funcblock->left;
    annotation = funcblock->right;
    funcdef = holder->left;
    body = holder->right;
    comment = (AST *)funcblock->d.ptr;

    language = holder->d.ival;
    
    if (funcdef->kind != AST_FUNCDEF || funcdef->left->kind != AST_FUNCDECL) {
        ERROR(funcdef, "Internal error, bad function definition");
        return NULL;
    }
    src = funcdef->left;
    is_cog = CODE_PLACE_DEFAULT;
    if (gl_p2 && FindAnnotation(annotation, "lut") != 0) {
        is_cog = CODE_PLACE_LUT;
        gl_have_lut++;
    } else if (FindAnnotation(annotation, "cog") != 0) {
        is_cog = CODE_PLACE_COG;
    } else if (FindAnnotation(annotation, "hub") != 0) {
        is_cog = CODE_PLACE_HUB;
    }
    srcname = src->left;
    if (srcname->kind == AST_LOCAL_IDENTIFIER) {
        funcname_internal = srcname->left->d.string;
        funcname_user = srcname->right->d.string;
    } else if (srcname->kind == AST_IDENTIFIER) {
        funcname_internal = funcname_user = srcname->d.string;
    } else {
        ERROR(funcdef, "Internal error, no function name");
        return NULL;
    }
    /* look for an existing definition */
    sym = FindSymbol(&current->objsyms, funcname_internal);
    if (sym && sym->kind == SYM_VARIABLE) {
        AST *typ = (AST *)sym->val;
        if (typ->kind == AST_FUNCTYPE) {
            // We are redefining a bare function declaration in a class
            fdef = NewFunction(language);
            sym->val = (void *)fdef;
            sym->kind = SYM_FUNCTION;
        }
    }
    if (sym && sym->kind != SYM_WEAK_ALIAS) {
        if (sym->kind != SYM_FUNCTION) {
            ERROR(funcdef, "Redefining %s as a function or subroutine", funcname_user);
            return NULL;
        }
        fdef = (Function *)sym->val;
        oldtype = fdef->overalltype;
    } else {
        fdef = NewFunction(language);
        /* define the function symbol itself */
        /* note: static functions may get alias names, so we have to look
         * for an alias and declare under that name; that's what AliasName()
         * does
         */
        sym = AddSymbolPlaced(&current->objsyms, funcname_internal, SYM_FUNCTION, fdef, funcname_user, funcdef);
        if (!is_public) {
            sym->flags |= SYMF_PRIVATE;
        }
        sym->module = current;
    }
    
    if (FindAnnotation(annotation, "constructor")) fdef->attributes |= FUNC_ATTR_CONSTRUCTOR;
    if (FindAnnotation(annotation, "destructor"))  fdef->attributes |= FUNC_ATTR_DESTRUCTOR;
    if (FindAnnotation(annotation, "needsinit"))   fdef->attributes |= FUNC_ATTR_NEEDSINIT;
    if (FindAnnotation(annotation, "complexio"))   fdef->attributes |= FUNC_ATTR_COMPLEXIO;

    if (fdef->body) {
        // we already saw a definition for the function; if this was just
        // an alias then it may be OK
        if (fdef->body->kind == AST_STRING) {
            if (body && body->kind == AST_STRING) {
                if (0 != strcmp(fdef->body->d.string, body->d.string)) {
                    ERROR(funcdef, "different __fromfile strings for function %s", funcname_user);
                }
                return fdef; // nothing else we need to do here
            }
        } else {
            if (body && body->kind == AST_STRING) {
                /* providing a __fromfile() declaration after we saw
                   a real declaration; just ignore it */
                return fdef;
            } else if (!AstMatch(fdef->decl, funcdef) || !AstBodyMatch(fdef->body, body)) {
                ERROR(funcdef, "redefining function or subroutine %s", funcname_user);
                NOTE(fdef->decl, "the previous definition is here");
            } else {
                if (DifferentLineNumbers(funcdef, fdef->decl)) {
                    WARNING(funcdef, "duplicate definition for %s", funcname_user);
                    NOTE(fdef->decl, "the previous definition is here");
                }
            }
        }
        // if we get here then we are redefining a previously seen __fromfile
        // ignore the old stub function and start with a fresh one
        // BEWARE: there's some stuff (like fdef->next) we do not want to lose
        ReinitFunction(fdef, fdef->language);
    }

    {
        const char *opts = FindAnnotation(annotation, "opt");
        if (opts) {
            //printf("Optimize string: [%s]\n", opt);
            if (opts[0] != '(') {
                ERROR(annotation, "optimization options must be enclosed in parentheses");
            } else {
                ParseOptimizeString(annotation, opts+1, &fdef->optimize_flags);
            }
        }
    }
    if (FindAnnotation(annotation, "noinline") != 0) {
        fdef->no_inline = 1;
    }
    {
        const char *spfunc = FindAnnotation(annotation, "specialfunc");
        if (spfunc) {
            //printf("Optimize string: [%s]\n", opt);
            if (spfunc[0] != '(') {
                ERROR(annotation, "special function options must be enclosed in parentheses");
            } else {
                char *tmp;
                spfunc = tmp = strdup(spfunc+1);
                while (tmp && *tmp) {
                    if (*tmp == ')') {
                        *tmp = 0;
                        break;
                    }
                    tmp++;
                }
                fdef->specialfunc = spfunc;
            }
        }
    }
    
    fdef->name = funcname_internal;
    fdef->user_name = funcname_user;
    fdef->annotations = annotation;
    fdef->decl = funcdef;
    fdef->language = language;
    fdef->code_placement = is_cog;
    if (is_cog != CODE_PLACE_DEFAULT) {
        // if the code placement is explicit and differs from where we
        // would normally put code, make sure we do not inline the
        // function
        fdef->no_inline = 1;
    }
    if (comment) {
        if (comment->kind != AST_COMMENT && comment->kind != AST_SRCCOMMENT) {
            ERROR(comment, "Internal error, expected comment");
            abort();
        }
        fdef->doccomment = comment;
    }
    fdef->is_public = is_public;
    fdef->overalltype = NewAST(AST_FUNCTYPE, NULL, NULL);
    retinfo = src->right;
    if (retinfo->kind == AST_RETURN) {
        src = retinfo; // so src->right holds returned variables
        fdef->overalltype->left = retinfo->left;
    } else {
        ERROR(funcdef, "Internal error in function declaration structure");
    }
    fdef->numresults = 1;
    fdef->result_declared = (src->right != NULL); // were number of results declared?
    if (fdef->overalltype->left) {
        fdef->result_declared = 1;
        if (fdef->overalltype->left == ast_type_void) {
            fdef->numresults = 0;
        } else if (fdef->overalltype->left->kind == AST_TUPLE_TYPE) {
            fdef->numresults = AstListLen(fdef->overalltype->left);
        } else {
            int siz;
            if (fdef->overalltype->left->kind == AST_INTEGER) {
                ERROR(funcdef, "Bad function declaration for %s", fdef->name);
                fdef->overalltype->left = ast_type_generic;
            }
            siz = TypeSize(fdef->overalltype->left);
            if (TypeGoesOnStack(fdef->overalltype->left)) {
                fdef->numresults = 1; // will return a pointer
            } else {
                siz = (siz + 3) & ~3;
                fdef->numresults = siz / 4;
            }
        }
    }
    if (!src->right || src->right->kind == AST_RESULT) {
        if (IsSpinLang(language)) {
            fdef->resultexpr = AstIdentifier("result");
            AddSymbolPlaced(&fdef->localsyms, "result", SYM_RESULT, NULL, NULL, src);
        } else {
            fdef->resultexpr = AstIdentifier("__result");
            AddSymbol(&fdef->localsyms, "__result", SYM_RESULT, NULL, NULL);
        }
    } else {
        AST *resultexpr = src->right;
        AST *type = NULL;
        fdef->resultexpr = resultexpr;
        if (resultexpr->kind == AST_DECLARE_VAR) {
            fdef->overalltype->left = type = resultexpr->left;
            resultexpr = resultexpr->right;
        }
        if (resultexpr->kind == AST_IDENTIFIER) {
            AddSymbolPlaced(&fdef->localsyms, resultexpr->d.string, SYM_RESULT, type, NULL, resultexpr);
        } else if (resultexpr->kind == AST_LISTHOLDER) {
            AST *rettype;
            int count;
            count = fdef->numresults = EnterVars(SYM_RESULT, &fdef->localsyms, NULL, resultexpr, 0, 0, 0) / LONG_SIZE;
            rettype = NULL;
            while (count > 0) {
                rettype = NewAST(AST_TUPLE_TYPE, NULL, rettype);
                --count;
            }
            fdef->overalltype->left = rettype;
            fdef->resultexpr = TranslateToExprList(fdef->resultexpr);
        } else {
            ERROR(funcdef, "Internal error, bad contents of return value field");
        }
    }

    vars = funcdef->right;
    if (vars->kind != AST_FUNCVARS) {
        ERROR(vars, "Internal error, bad variable declaration");
    }

    /* enter the variables into the local symbol table */
    fdef->params = vars->left;
    fdef->locals = vars->right;
    /* and into the parameter list */
    fdef->overalltype->right = fdef->params;
    
    /* set up default values for parameters, if any present */
    {
        AST *a, *p;
        AST *defval;
        
        defparams = NULL;
        for (a = fdef->params; a; a = a->right) {
            AST **aptr = &a->left;
            p = a->left;
            if (p->kind == AST_DECLARE_VAR) {
                aptr = &p->right;
                p = p->right;
            }
            if (p && p->kind == AST_ASSIGN) {
                *aptr = p->left;
                defval = p->right;
                if (!IsConstExpr(defval) && !IsStringConst(defval)) {
                    ERROR(defval, "default parameter value must be constant");
                }
            } else {
                defval = NULL;
            }
            defparams = AddToList(defparams, NewAST(AST_LISTHOLDER, defval, NULL));
        }
        fdef->defaultparams = defparams;
    }

    curfunc = fdef;

    // in C parameters declared as arrays should actually be treated as pointers
    // similarly in BASIC for objects
    AdjustParameterTypes(fdef->params, fdef->language);

    fdef->numparams = EnterVars(SYM_PARAMETER, &fdef->localsyms, NULL, fdef->params, 0, 0, 0) / LONG_SIZE;
    /* check for VARARGS */
    {
        AST *arg;
        for (arg = fdef->params; arg; arg = arg->right) {
            if (arg->left->kind == AST_VARARGS) {
                if (fdef->numparams == 0) {
                    ERROR(fdef->params, "varargs function must have at least one parameter");
                }
                fdef->numparams = -fdef->numparams;
            }
        }
    }
    
    fdef->numlocals = EnterVars(SYM_LOCALVAR, &fdef->localsyms, NULL, fdef->locals, 0, 0, 0) / LONG_SIZE;

    // if there are default values for the parameters, use those to initialize
    // the symbol types (only if we need to)
    if (fdef->defaultparams) {
        AST *vals = fdef->defaultparams;
        AST *params = fdef->params;
        AST *id;
        AST *defval;
        while (params && vals) {
            id = params->left;
            params = params->right;
            defval = vals->left;
            vals = vals->right;
            if (id->kind == AST_IDENTIFIER) {
                Symbol *sym = FindSymbol(&fdef->localsyms, id->d.string);
                if (sym && sym->kind == SYM_PARAMETER && !sym->val) {
                    sym->val = (void *)ExprType(defval);
                }
            }
        }
    }

    /* if there was an old definition, validate it */
    if (oldtype && oldtype->left) {
        if (!CompatibleTypes(oldtype, fdef->overalltype)) {
            WARNING(funcdef, "Redefining function or subroutine %s with an incompatible type", funcname_user);
        }
    }
    fdef->body = body;

    /* declare any local variables in the function */
    findLocalsAndDeclare(fdef, body);

    // copy the local definitions over to the closure
    if (fdef->closure) {
        AST *varlist;
        AST *superclass;
        if (gl_interp_kind != INTERP_KIND_NUCODE) {
            if (fdef->numresults > 0) {
                AddClosureSymbol(fdef, fdef->closure, fdef->resultexpr);
            }
        }
        for (varlist = fdef->params; varlist; varlist = varlist->right) {
            AddClosureSymbol(fdef, fdef->closure, varlist->left);
        }
        if (gl_interp_kind == INTERP_KIND_NUCODE) {
            AddClosureSymbol(fdef, fdef->closure, AstIdentifier("__interp_reserved1__"));
            AddClosureSymbol(fdef, fdef->closure, AstIdentifier("__interp_reserved2__"));
            AddClosureSymbol(fdef, fdef->closure, AstIdentifier("__interp_reserved3__"));
            AddClosureSymbol(fdef, fdef->closure, AstIdentifier("__interp_reserved4__"));
            if (fdef->numresults > 0) {
                AddClosureSymbol(fdef, fdef->closure, fdef->resultexpr);
            }
        }
        for (varlist = fdef->locals; varlist; varlist = varlist->right) {
            AddClosureSymbol(fdef, fdef->closure, varlist->left);
        }
        // finally add a definition for the super class
        superclass = AstIdentifier("__super");
        superclass = NewAST(AST_DECLARE_VAR,
                            NewAST(AST_PTRTYPE,
                                   ClassType(fdef->module), 
                                   NULL),
                            superclass);
        AddClosureSymbol(fdef, fdef->closure, superclass);
    }
    // restore function symbol environment (if applicable)
    curfunc = oldcur;
    return fdef;
}

void
DeclareFunctions(Module *P)
{
    AST *ast;
    Module *savecurrent = current;
    current = P;
    ast = P->funcblock;
    while (ast) {
        doDeclareFunction(ast->left);
        ast = ast->right;
    }
    P->funcblock = NULL;
    current = savecurrent;
}

/*
 * convert lookup/lookdown into a case statement
 * lookdown(i : a, b, c..d, e) -> 
 *      case i
 *        a: r := 1
 *        b: r := 2
 *        c..d: r:= 3+(i-c)
 *        e: r := 4+(d-c)
 * lookup(i : a, b, c..d, e) -> 
 *      case i
 *        1: r := a
 *        2: r := b
 *        3..3+(d-c): r:= c+(i-3)
 *        4+(d-c): r := e
 */
/* helper function to generate a case item */
static AST *GenCase(int isLookup, AST *itembase, AST *itemlimit, AST *lookupvar, AST *index)
{
    AST *caseitem;
    AST *casestmt;
    AST *expr;
    if (isLookup) {
        if (itemlimit) {
            caseitem = AstOperator('+', index, AstOperator('-', itemlimit, itembase));
            caseitem = NewAST(AST_RANGE, index, SimpleOptimizeExpr(caseitem));
            caseitem = NewAST(AST_ISBETWEEN, lookupvar, caseitem);
            expr = SimpleOptimizeExpr(AstOperator('+', itembase, AstOperator('-', lookupvar, index)));
            casestmt = expr;
        } else {
            caseitem = AstOperator(K_EQ, lookupvar, index);
            casestmt = itembase;
        }
    } else {
        if (itemlimit) {
            caseitem = NewAST(AST_RANGE, itembase, itemlimit);
            caseitem = NewAST(AST_ISBETWEEN, lookupvar, caseitem);
            expr = AstOperator('+', index, AstOperator('-', lookupvar, itembase));
            casestmt = SimpleOptimizeExpr(expr);
        } else {
            caseitem = AstOperator(K_EQ, lookupvar, itembase);
            casestmt = index;
        }
    }

    // now generate: (caseitem) ? casestmt : NULL
    casestmt = NewAST(AST_THENELSE, casestmt, NULL);
    casestmt = NewAST(AST_CONDRESULT, caseitem, casestmt);
    return casestmt;
}

/* add elseclause to an existing x ? y : z clause */
static AST *
AddToIf(AST *ast, AST *newclause)
{
    AST *x = newclause->right;
    if (x->kind != AST_THENELSE || x->right != NULL) {
        WARNING(newclause, "Internal error, bad else clause");
    } else {
        x->right = ast;
    }
    return newclause;
}

/* main function */
static AST *
LookupAsCase(AST *top)
{
    AST *lookexpr = top->left;
    AST *list = top->right;
    AST *index = lookexpr->left;
    AST *lookvar = lookexpr->right;
    AST *expr;
    AST *thisif;
    AST *stmtlist = AstInteger(0);
    AST *one = AstInteger(1);
    int isLookup = top->kind == AST_LOOKUP;
    AST *newvar = NULL;
    
    if (!IsIdentifier(lookvar)) {
        newvar = AstTempVariable("_look_");
        AddLocalVariable(curfunc, newvar, NULL, SYM_LOCALVAR);
        lookvar = newvar;
    }
    while (list) {
        expr = list->left;
        list = list->right;
        if (expr->kind == AST_RANGE) {
            thisif = GenCase(isLookup, expr->left, expr->right, lookvar, index);
            stmtlist = AddToIf(stmtlist, thisif);
            index = AstOperator('+', index, AstOperator('+', one, AstOperator('-', expr->right, expr->left)));
        } else if (expr->kind == AST_STRING) {
            ERROR(expr, "Not equipped to handle this yet");
        } else {
            stmtlist = AddToIf(stmtlist, GenCase(isLookup, expr, NULL, lookvar, index));
            index = AstOperator('+', index, one);
        }
        index = SimpleOptimizeExpr(index);
    }
    if (lookvar == newvar) {
        stmtlist = NewAST(AST_SEQUENCE, AstAssign(lookvar, lookexpr->right), stmtlist);
    }
    return stmtlist;
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
    int badlen = 0;
    
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

    // Bytecode has native lookup/lookdown
    if (TraditionalBytecodeOutput()) return NULL;

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
        } else if (expr->kind == AST_STRING) {
            len += strlen(expr->d.string);
        } else {
            if (IsConstExpr(expr)) {
                len++;
            } else {
                badlen = 1;
                len++;
            }
        }
    }

    /* if we get here, the array is constant of length "len" */
    if (len > 256) {
        *top = *LookupAsCase(top);
        return NULL;
    }
    if (badlen) {
        return NULL;
    }
    /* create a temporary identifier for it */
    id = AstTempVariable("look_");
    /* replace the table in the top expression */
    top->right = NewAST(AST_TEMPARRAYUSE, id, AstInteger(len));

    /* create a declaration */
    decl = NewAST(AST_TEMPARRAYDECL, NewAST(AST_ARRAYDECL, id, AstInteger(len)), table);

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
 * (3) Check for using a variable before initializing it
 *
 * Called recursively; the top level call has ast = func->body
 * Returns an AST that should be printed before the function body, e.g.
 * to declare temporary arrays.
 */
enum SetKind { SETFLAG_NEVER = 0, SETFLAG_ALWAYS = 3 };

static AST *
NormalizeFunc(AST *ast, Function *func, enum SetKind setflag)
{
    AST *ldecl;
    AST *rdecl;
    Symbol *sym;
    const char *name;
    
    if (!ast)
        return NULL;

    switch (ast->kind) {
    case AST_RETURN:
        if (ast->left) {
            return NormalizeFunc(ast->left, func, SETFLAG_NEVER);
        }
        return NULL;
    case AST_ADDROF:
    case AST_ABSADDROF:
        return NormalizeFunc(ast->left, func, SETFLAG_ALWAYS);
    case AST_RESULT:
        func->result_used = 1;
        return NULL;
    case AST_METHODREF:
        return NormalizeFunc(ast->left, func, setflag);
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER:
        rdecl = func->resultexpr;
        if (rdecl && rdecl->kind == AST_DECLARE_VAR) {
            rdecl = rdecl->right;
        }
        if (rdecl && AstUses(rdecl, ast))
            func->result_used = 1;
        sym = LookupSymbol(GetIdentifierName(ast));
        if (sym && sym->kind == SYM_LOCALVAR) {
            switch (setflag) {
            case SETFLAG_ALWAYS:
                sym->flags |= SYMF_INITED;
                break;
            default:
                if (!(sym->flags & SYMF_INITED)) {
                    if (gl_warn_flags & WARN_UNINIT_VARS) {
                        WARNING(ast, "%s is possibly used before initialization", sym->user_name);
                    }
                }
                break;
            }
        }
        return NULL;
    case AST_INTEGER:
    case AST_FLOAT:
    case AST_STRING:
    case AST_STRINGPTR:
    case AST_CONSTANT:
    case AST_HWREG:
    case AST_CONSTREF:
    case AST_SIZEOF:
        return NULL;
    case AST_LOOKUP:
    case AST_LOOKDOWN:
        ldecl = NormalizeFunc(ast->left, func, SETFLAG_NEVER);
        rdecl = NormalizeFunc(ast->right, func, SETFLAG_NEVER);
        return ModifyLookup(ast);
    case AST_VA_START:
    case AST_ASSIGN:
        rdecl = NormalizeFunc(ast->right, func, SETFLAG_NEVER);
        if (0 != (name = GetIdentifierName(ast->left))) {
            sym = LookupSymbol(name);
            if (sym) {
                sym->flags |= SYMF_INITED;
            }
        }
        ldecl = NormalizeFunc(ast->left, func, SETFLAG_ALWAYS);
        return AddToList(ldecl, rdecl);
    case AST_DOWHILE:
        rdecl = NormalizeFunc(ast->right, func, setflag);
        ldecl = NormalizeFunc(ast->left, func, setflag);
        return AddToList(ldecl, rdecl);
    case AST_COUNTREPEAT:
        ldecl = NormalizeFunc(ast->left, func, SETFLAG_ALWAYS); // in a COUNTREPEAT the variable is initialized
        rdecl = NormalizeFunc(ast->right, func, setflag);
        return AddToList(ldecl, rdecl);
    case AST_FOR:
    case AST_FORATLEASTONCE:
        // initialization statement
        ldecl = NormalizeFunc(ast->left, func, setflag);
        {
            AST *loopcond = ast->right->left;
            AST *update = ast->right->right->left;
            AST *body = ast->right->right->right;
            ldecl = AddToList(ldecl, NormalizeFunc(loopcond, func, setflag));
            ldecl = AddToList(ldecl, NormalizeFunc(body, func, setflag));
            ldecl = AddToList(ldecl, NormalizeFunc(update, func, setflag));
            return ldecl;
        }
    case AST_DECLARE_VAR:
        /* avoid spurious use-before-set warnings */
        return NULL;
    case AST_INLINEASM:
        /* assume declared result variables are used in
           inline assembly */
        if (func->result_declared) {
            func->result_used = 1;
        }
        setflag = SETFLAG_ALWAYS;  /* assume stuff gets set before use in inline assembly */
        /* otherwise, fall through */
    default:
        ldecl = NormalizeFunc(ast->left, func, setflag);
        rdecl = NormalizeFunc(ast->right, func, setflag);
        return AddToList(ldecl, rdecl);
    }
}

/*
 * find the variable symbol in an array declaration or identifier
 */
Symbol *VarSymbol(Function *func, AST *ast)
{
    if (ast && ast->kind == AST_ARRAYDECL)
        ast = ast->left;
    if (ast == 0 || ast->kind != AST_IDENTIFIER) {
        ERROR(ast, "Internal error, expected variable name\n");
        return NULL;
    }
    return FindSymbol(&func->localsyms, ast->d.string);
}

/*
 * add a local variable to a function
 */
void
AddLocalVariable(Function *func, AST *var, AST *vartype, int symtype)
{
    AST *varlist = NewAST(AST_LISTHOLDER, var, NULL);
    int numlongs;
    
    if (!vartype) {
        vartype = ast_type_long;
    }
    EnterVars(symtype, &func->localsyms, vartype, varlist, func->numlocals * LONG_SIZE, 0, 0);
    func->locals = AddToList(func->locals, NewAST(AST_LISTHOLDER, var, NULL));
    if (vartype) {
        numlongs = (TypeSize(vartype) + 3) / 4;
    } else {
        numlongs = 1;
    }
    func->numlocals += numlongs;
    if (func->localarray) {
        func->localarray_len += numlongs;
    }
}

int  GetTempStack(void) { return curfunc->local_var_counter; }
void SetTempStack(int n) { curfunc->local_var_counter = n; }

AST *
AstTempLocalVariable(const char *prefix, AST *type)
{
    char *name;
    AST *ast = NewAST(AST_IDENTIFIER, NULL, NULL);

    if (curfunc && (!type || TypeSize(type) == LONG_SIZE)) {
        name = NewTemporaryVariable(prefix, &curfunc->local_var_counter);
        type = ast_type_generic; /* make it untyped so it can be re-used */
    } else {
        name = NewTemporaryVariable(prefix, NULL);
    }
    ast->d.string = name;
    AddLocalVariable(curfunc, ast, type, SYM_TEMPVAR);
    return ast;
}

/*
 * transform a case expression list into a single boolean test
 * "var" is the variable the case is testing against (may be
 * a temporary)
 */
AST *
TransformCaseExprList(AST *var, AST *ast)
{
    AST *listexpr = NULL;
    AST *node = NULL;
    ASTReportInfo saveinfo;
    AST *result;
    
    while (ast) {
        AstReportAs(ast, &saveinfo);
        if (ast->kind == AST_OTHER) {
            result = AstInteger(1);
            AstReportDone(&saveinfo);
            return result;
        }
        if (ast->left->kind == AST_RANGE) {
            node = NewAST(AST_ISBETWEEN, var, ast->left);
        } else if (ast->left->kind == AST_STRING) {
            // if the string is long, break it up into single characters
            const char *sptr = ast->left->d.string;
            int c;
            while ( (c = *sptr++) != 0) {
                char *newStr = (char *)malloc(2);
                AST *strNode;
                newStr[0] = c; newStr[1] = 0;
                strNode = NewAST(AST_STRING, NULL, NULL);
                strNode->d.string = newStr;
                node = AstOperator(K_EQ, var, strNode);
                if (*sptr) {
                    if (listexpr) {
                        listexpr = AstOperator(K_BOOL_OR, listexpr, node);
                    } else {
                        listexpr = node;
                    }
                }
            }
        } else {
            node = AstOperator(K_EQ, var, ast->left);
        }
        if (listexpr) {
            listexpr = AstOperator(K_BOOL_OR, listexpr, node);
        } else {
            listexpr = node;
        }
        ast = ast->right;
        AstReportDone(&saveinfo);
    }
    return listexpr;
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
        if (gl_output == OUTPUT_CPP)
            gl_output = OUTPUT_C;
    }
}

/*
 * an annotation just looks like a function with no name or body
 */
void
DeclareToplevelAnnotation(AST *anno)
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
        f = NewFunction(0);
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
            if (sym->kind == SYM_VARIABLE && !(sym->flags & SYMF_GLOBAL)) {
                fdef->is_static = 0;
                return;
            }
            if (sym->kind == SYM_FUNCTION) {
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
    case AST_COGINIT:
        // if we end up doing a COGINIT of a non-static member,
        // then we better be non-static as well. This could be
        // optimized further, but for now punt
        if (IsSpinCoginit(body, NULL)) {
            fdef->is_static = 0;
        }
        CheckForStatic(fdef, body->left);
        CheckForStatic(fdef, body->right);
        break;
    default:
        CheckForStatic(fdef, body->left);
        CheckForStatic(fdef, body->right);
        break;
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
        if (ast->kind == AST_COMMENTEDNODE) ast = ast->left;
        ASSERT_AST_KIND(ast,AST_STMTLIST,return 0;);
        sawreturn |= CheckRetStatement(func, ast->left);
        ast = ast->right;
    }
    return sawreturn;
}

static bool
IsResultVar(Function *func, AST *lhs)
{
    if (lhs->kind == AST_RESULT) {
        if (func->resultexpr && func->resultexpr->kind == AST_EXPRLIST) {
            ERROR(lhs, "Do not use RESULT in functions returning multiple values");
            return false;
        }
        return true;
    }
    if (lhs->kind == AST_IDENTIFIER) {
        AST *resultexpr = func->resultexpr;
        if (resultexpr->kind == AST_DECLARE_VAR) {
            resultexpr = resultexpr->right;
        }
        return AstMatch(lhs, resultexpr);
    }
    return false;
}

static int
CheckRetCaseMatchList(Function *func, AST *ast)
{
    // W21: I think this may have bitrotted to some extent?
    return CheckRetStatementList(func,ast->right);
    /*
    AST *item;
    int sawReturn = 1;
    while (ast) {
        ASSERT_AST_KIND(ast,AST_STMTLIST,return sawReturn;);

        item = ast->left;
        ast = ast->right;
        ASSERT_AST_KIND(item,AST_CASEITEM,return sawReturn;);
        if (item->right && item->right->kind == AST_STMTLIST) {
            sawReturn = CheckRetStatementList(func, item->right) && sawReturn;
        } else {
            sawReturn = CheckRetStatement(func,item->right) && sawReturn;
        }
    }
    return sawReturn;*/
}

static AST *
ForceExprType(AST *ast)
{
    AST *typ;
    typ = ExprType(ast);
    if (!typ)
        typ = ast_type_generic;
    return typ;
}

static int
CheckRetJumpList(Function *func, AST *ast)
{
    int sawret = 1;
    while (ast && ast->kind == AST_LISTHOLDER) {
        ast = ast->right;
    }
    while (ast && ast->kind == AST_STMTLIST) {
        // order of && below matters, we do want to check
        // return types in case some branches return a value
        // and others do not
        sawret = CheckRetStatement(func, ast->left) && sawret;
        ast = ast->right;
    }
    return sawret;
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
            SetFunctionReturnType(func, ForceExprType(ast->left));
        }
        sawreturn = 1;
        break;
    case AST_THROW:
        if (ast->left) {
            (void)CheckRetStatement(func, ast->left);
            SetFunctionReturnType(func, ForceExprType(ast->left));
        }
        func->has_throw = 1;
        break;
    case AST_IF:
        ast = ast->right;
        if (ast->kind == AST_COMMENTEDNODE)
            ast = ast->left;
        sawreturn = CheckRetStatementList(func, ast->left);
        sawreturn = CheckRetStatementList(func, ast->right) && sawreturn;
        break;
    case AST_JUMPTABLE:
        sawreturn = CheckRetJumpList(func, ast->right);
        break;
    case AST_CASE:
        sawreturn = CheckRetCaseMatchList(func, ast->right);
        break;
    case AST_WHILE:
    case AST_DOWHILE:
        sawreturn = CheckRetStatementList(func, ast->right);
        break;
    case AST_COUNTREPEAT:
        lhs = ast->left; // count variable
        if (lhs) {
            if (IsResultVar(func, lhs)) {
                SetFunctionReturnType(func, ast_type_long);
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
            // watch out for multiple returns
            AST *typ = ForceExprType(rhs);
            if (typ && typ->kind != AST_TUPLE_TYPE) {
                SetFunctionReturnType(func, typ);
            }
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
 * expand arguments in a SEND type function call
 */
AST *
ExpandArguments(AST *sendptr, AST *args)
{
    AST *seq = NULL;
    AST *arg;
    AST *call;
    AST *typ;
    static AST *sendstring = NULL;

    call = NULL;
    while (args) {
        arg = args->left;
        args = args->right;
        if (arg) {
            switch (arg->kind) {
            case AST_STRING:
                if (!sendstring) {
                    sendstring = AstIdentifier("__sendstring");
                }
                arg = NewAST(AST_EXPRLIST, arg, NULL);
                arg = NewAST(AST_STRINGPTR, arg, NULL);
                arg = NewAST(AST_EXPRLIST, arg, NULL);
                call = NewAST(AST_FUNCCALL, sendstring, arg);
                break;
            case AST_FUNCCALL:
                typ = ExprType(arg);
                if (!typ) {
                    // hmmm, foo(x) might be a Spin void function
                    // we need to check for that case
                    if (IsIdentifier(arg->left)) {
                        Symbol *sym = LookupAstSymbol(arg->left, NULL);
                        if (sym && sym->kind == SYM_FUNCTION) {
                            Function *f = (Function *)sym->val;
                            if (f->numresults == 0 || !f->result_declared) {
                                typ = ast_type_void;
                            }
                        }
                    }
                }
                if (typ == ast_type_void) {
                    call = arg;
                } else {
                    // FIXME: do we need to handle multiple return values
                    // here??
                    call = NewAST(AST_FUNCCALL, sendptr,
                                  NewAST(AST_EXPRLIST, arg, NULL));
                }
                break;
            case AST_SEQUENCE:
                /* ignore sequence like (f(x),0), which we
                   generated before for a void f; just do f(x) */
                if (arg->left && arg->left->kind == AST_FUNCCALL
                    && arg->right && arg->right->kind == AST_INTEGER
                    && arg->right->d.ival == 0
                    && ExprType(arg->left) == ast_type_void)
                {
                    call = arg->left;
                    break;
                }
                /* otherwise fall through */
            default:
                call = NewAST(AST_FUNCCALL, sendptr,
                              NewAST(AST_EXPRLIST, arg, NULL));
                break;
            }
        }
        if (seq) {
            seq = NewAST(AST_SEQUENCE, seq, call);
        } else {
            seq = call;
        }
    }
    return seq;
}

/*
 * count number of (actual) parameters passed to a function
 * (e.g. 64 bit parameters count as two parameters)
 */
int
CountParameters(AST *list)
{
    AST *param;
    int n = 0;
    while (list) {
        param = list->left;
        list = list->right;
        n += NumExprItemsOnStack(param);
    }
    return n;
}

/*
 * check function calls for correct number of arguments
 * also does expansion for multiple returns used as parameters
 * and does default parameter substitution
 */
void
CheckFunctionCalls(AST *ast)
{
    int expectArgs;
    int gotArgs;
    int is_varargs = 0;
    Symbol *sym;
    const char *fname = "function";
    Function *f = NULL;
    int i, n;
    AST *initseq = NULL;
    AST *temps[MAX_TUPLE];
    ASTReportInfo saveinfo;

    while (ast && ast->kind == AST_STMTLIST) {
        AST *sub = ast->left;
        int mark = GetTempStack();
        CheckFunctionCalls(sub);
        ast = ast->right;
        SetTempStack(mark);
    }
    if (!ast) {
        return;
    }
    // we may need to create temporaries
    AstReportAs(ast, &saveinfo);
    if (ast->kind == AST_FUNCCALL) {
        AST *a;
        AST **lastaptr;
        int doExpandArgs = 0;
        AST *typ;
        sym = FindCalledFuncSymbol(ast, NULL, 0);
        expectArgs = 0;
        if (sym) {
            fname = sym->user_name;
            if (sym->kind == SYM_BUILTIN) {
                Builtin *b = (Builtin *)sym->val;
                expectArgs = b->numparameters;
            } else if (sym->kind == SYM_FUNCTION) {
                f = (Function *)sym->val;
                expectArgs = f->numparams;
            } else {
                AST *ftype = ExprType(ast->left);
                if (ftype) {
                    if (ftype->kind == AST_MODIFIER_SEND_ARGS) {
                        doExpandArgs = 1;
                        ftype = ftype->left;
                    }
                    if (!IsFunctionType(ftype)) {
                        ERROR(ast, "%s is not a function but is being called", fname);
                        return;
                    }
                    if (ftype && ftype->kind == AST_PTRTYPE) {
                        ftype = ftype->left;
                    }
                }
                if (ftype) {
                    expectArgs = CountParameters(ftype->right);
                } else {
                    expectArgs = 0;
                    is_varargs = 1;
                }
                if (expectArgs == 0 && curfunc && IsCLang(curfunc->language)) {
                    is_varargs = 1;
                }
            }
        } else {
            goto skipcheck;
        }
        if (doExpandArgs) {
            int numArgs = AstListLen(ast->right);
            if (numArgs == 1) {
                if (ast->right->kind == AST_EXPRLIST) {
                    AST *subexpr = ast->right->left;
                    if (subexpr && subexpr->kind == AST_STRING) {
                        numArgs = strlen(subexpr->d.string);
                    } else if (subexpr && subexpr->kind == AST_FUNCCALL) {
                        numArgs = 2; // force ExpandArguments
                    }
                }
            }
            if (numArgs > 1) {
                initseq = ExpandArguments(ast->left, ast->right);
                *ast = *initseq;
            }
            goto skipcheck;
        }
        if (expectArgs < 0) {
            expectArgs = -expectArgs;
            is_varargs = 1;
        }
        gotArgs = 0;
        lastaptr = &ast->right;
        a = ast->right;
        while (a) {
            n = NumExprItemsOnStack(a->left);
            if (n > 1 && gl_output != OUTPUT_BYTECODE) {
                AST *lhsseq = NULL;
                AST *assign;
                AST *newparams;
                AST *exprlist;
                
		if (n > MAX_TUPLE) {
		  ERROR(ast, "argument too large to pass on stack");
		  n = MAX_TUPLE-1;
		}
                // many backends need the results placed in temporaries
                for (i = 0; i < n; i++) {
                    temps[i] = AstTempLocalVariable("_parm_", NULL);
                    lhsseq = AddToList(lhsseq, NewAST(AST_EXPRLIST, temps[i], NULL));
                }
                // create the new parameters
                newparams = DupAST(lhsseq);
                // create an initialization for the temps
                exprlist = a->left;
                typ = ExprType(a->left);
                
                if (IsIdentifier(exprlist)) {
                    AST *temp;
                    AST *id = exprlist;
                    Module *P = NULL;
                    if (typ && IsClassType(typ) && 0 != (P=GetClassPtr(typ))) {
                        Symbol *sym;
                        exprlist = NULL;
                        for (i = 0; i < n; i++) {
                            sym = FindSymbolByOffsetAndKind(&P->objsyms, i*LONG_SIZE, SYM_VARIABLE);
                            if (!sym || sym->kind != SYM_VARIABLE) {
                                ERROR(ast, "Internal error, couldn't find object variable with offset %d", i*LONG_SIZE);
                                return;
                            }
                            temp = NewAST(AST_METHODREF, id, AstIdentifier(sym->our_name));
                            temp = NewAST(AST_EXPRLIST, temp, NULL);
                            exprlist = AddToList(exprlist, temp);
                        }
                    } else if (typ && IsScalar64Type(typ)) {
                        exprlist = NewAST(AST_GETLOW, a->left, NULL);
                        exprlist = NewAST(AST_EXPRLIST, exprlist, NULL);
                        temp = NewAST(AST_GETHIGH, a->left, NULL);
                        temp = NewAST(AST_EXPRLIST, temp, NULL);
                        exprlist = AddToList(exprlist, temp);
                    } else {
                        ERROR(exprlist, "Internal error, Unable to handle this type of parameter");
                        return;
                    }
                }
                assign = AstAssign(lhsseq, exprlist);
                if (initseq) {
                    initseq = NewAST(AST_SEQUENCE, initseq, assign);
                } else {
                    initseq = NewAST(AST_SEQUENCE, assign, NULL);
                }
                // insert the new parameters into the list
                *lastaptr = newparams;
                {
                    AST *savea = a;
                    for (a = newparams; a->right; a = a->right)
                        ;
                    // now a is the last item
                    a->right = savea->right;
                }
            }
            lastaptr = &a->right;
            a = *lastaptr;
            gotArgs += n;
        }
        if (gotArgs < expectArgs) {
            // see if there are default values, and if so, insert them
            if (f && f->defaultparams) {
                AST *extra = f->defaultparams;
                int n = gotArgs;
                // skip first n items in the defaultparams list
                while (extra && n > 0) {
                    extra = extra->right;
                    --n;
                }
                while (extra) {
                    if (extra->left) {
                        a = NewAST(AST_EXPRLIST, extra->left, NULL);
                        ast->right = AddToList(ast->right, a);
                        gotArgs++;
                    }
                    extra = extra->right;
                }
            }
        }
        if (is_varargs) {
            if (gotArgs < expectArgs) {
                ERROR(ast, "Bad number of parameters in call to %s: expected at least %d but found only %d", fname, expectArgs, gotArgs);
                return;
            }
        } else {
            if (gotArgs != expectArgs) {
                if (ast->left == basic_print_integer) {
                    if (gotArgs - expectArgs == 1) {
                        ast->left = basic_print_integer_2; goto func_ok;
                    } else if (gotArgs - expectArgs == 2) {
                        ast->left = basic_print_integer_3; goto func_ok;
                    } else if (gotArgs - expectArgs == 3) {
                        ast->left = basic_print_integer_4; goto func_ok;
                    }
                }
                if (f && IsCLang(f->language)) {
                    if (strcmp(f->name, "main") != 0) {
                        // don't warn for main()
                        WARNING(ast, "Bad number of parameters in call to %s: expected %d found %d", fname, expectArgs, gotArgs);
                    }
                } else {
                    ERROR(ast, "Bad number of parameters in call to %s: expected %d found %d", fname, expectArgs, gotArgs);
                }
                return;
            }
        }
    func_ok:
        if (initseq) {
            // modify the ast to hold the sequence and then the function call
            initseq = NewAST(AST_SEQUENCE, initseq,
                             NewAST(AST_FUNCCALL, ast->left, ast->right));
            ast->kind = AST_SEQUENCE;
            ast->left = initseq;
            ast->right = NULL;
        }
    }
skipcheck:    
    CheckFunctionCalls(ast->left);
    CheckFunctionCalls(ast->right);
    AstReportDone(&saveinfo);
}

/*
 * do basic processing of functions
 */
void
ProcessOneFunc(Function *pf)
{
    int sawreturn = 0;
    Module *savecurrent;
    Function *savefunc;
    AST *decls;
    int last_errors = gl_errors;
    
    if (pf->lang_processed)
        return;
    if (pf->body) {
        if (pf->body->kind == AST_STRING || pf->body->kind == AST_BYTECODE) {
            // dummy declaration, return for now
            return;
        }
    }
    savecurrent = current;
    savefunc = curfunc;
    current = pf->module;
    curfunc = pf;

    if (IsBasicLang(pf->language)) {
        BasicTransform(pf);
    } else if (IsCLang(pf->language)) {
        CTransform(pf);
    } else {
        SpinTransform(pf);
    }

    if (last_errors != gl_errors && gl_errors >= gl_max_errors) return;
    
    CheckRecursive(pf);  /* check for recursive functions */
    decls = NormalizeFunc(pf->body, pf, SETFLAG_NEVER);
    pf->extradecl = AddToList(pf->extradecl, decls);

    /* check for void functions */
    if (TraditionalBytecodeOutput()) {
        pf->result_used = 1;
    }
    if (pf->result_declared && pf->language == LANG_SPIN_SPIN2) {
        // there was an explicit result declared, so it is returned
        pf->result_used = 1;
    }
    sawreturn = CheckRetStatementList(pf, pf->body);

    if ( GetFunctionReturnType(pf) == NULL && (pf->result_used || (IsSpinLang(pf->language) && pf->has_throw)) ) {
        /* there really is a return type */
        SetFunctionReturnType(pf, ast_type_generic);
    }
    if (GetFunctionReturnType(pf) == NULL || GetFunctionReturnType(pf) == ast_type_void) {
        if (!pf->overalltype) {
            // this can happen for functions declared via annotations
            // (i.e. that don't really exist)
            pf->overalltype = NewAST(AST_FUNCTYPE, NULL, NULL);
        }
        pf->overalltype->left = ast_type_void;
        pf->numresults = 0;
        pf->resultexpr = NULL;
    } else {
        if (!pf->result_used) {
            pf->resultexpr = AstInteger(0);
            pf->result_used = 1;
            if (sawreturn && pf->numresults > 0) {
                LANGUAGE_WARNING(LANG_SPIN_SPIN2, pf->decl, "function %s returns a value but was declared without a return variable", pf->name);
            }
        }
        if (!sawreturn) {
            AST *retstmt;
            ASTReportInfo saveinfo;
            AstReportAs(pf->body, &saveinfo); // use old debug info
            retstmt = NewAST(AST_STMTLIST, NewAST(AST_RETURN, pf->resultexpr, NULL), NULL);
            AstReportDone(&saveinfo);
            pf->body = AddToList(pf->body, retstmt);
        }
    }
    
    CheckFunctionCalls(pf->body);
        
    pf->lang_processed = 1;
    current = savecurrent;
    curfunc = savefunc;
}

/*
 * do type inference on a statement list
 */
static int
InferTypesStmtList(AST *list)
{
  int changes = 0;
  if (list && (list->kind == AST_STRING || list->kind == AST_BYTECODE)) {
      return 0;
  }
  while (list) {
    if (list->kind != AST_STMTLIST) {
      ERROR(list, "Internal error, expected statement list");
      return 0;
    }
    changes |= InferTypesStmt(list->left);
    list = list->right;
  }
  return changes;
}

static int
InferTypesStmt(AST *ast)
{
  AST *sub;
  int changes = 0;

  if (!ast) return 0;
  switch(ast->kind) {
  case AST_COMMENTEDNODE:
    return InferTypesStmt(ast->left);
  case AST_ANNOTATION:
    return 0;
  case AST_RETURN:
    sub = ast->left;
    if (!sub) {
      sub = curfunc->resultexpr;
    }
    if (sub && sub->kind != AST_TUPLE_TYPE && sub->kind != AST_EXPRLIST) {
        changes = InferTypesExpr(sub, GetFunctionReturnType(curfunc));
    }
    return changes;
  case AST_IF:
    changes += InferTypesExpr(ast->left, NULL);
    ast = ast->right;
    if (ast->kind == AST_COMMENTEDNODE) {
      ast = ast->left;
    }
    changes += InferTypesStmtList(ast->left);
    changes += InferTypesStmtList(ast->right);
    return changes;
  case AST_WHILE:
  case AST_DOWHILE:
    changes += InferTypesExpr(ast->left, NULL);
    changes += InferTypesStmtList(ast->right);
    return changes;
  case AST_FOR:
  case AST_FORATLEASTONCE:
    changes += InferTypesExpr(ast->left, NULL);
    ast = ast->right;
    changes += InferTypesExpr(ast->left, NULL);
    ast = ast->right;
    changes += InferTypesExpr(ast->left, NULL);
    changes += InferTypesStmtList(ast->right);
    return changes;
  case AST_STMTLIST:
    return InferTypesStmtList(ast);
  case AST_SEQUENCE:
    changes += InferTypesStmt(ast->left);
    changes += InferTypesStmt(ast->right);
    return changes;
  case AST_ASSIGN:
  default:
    return InferTypesExpr(ast, NULL);
  }
}

static AST *
PtrType(AST *base)
{
  return NewAST(AST_PTRTYPE, base, NULL);
}

static AST *
WidenType(AST *newType)
{
    if (newType == ast_type_byte || newType == ast_type_word)
        return ast_type_long;
    return newType;
}

static int
SetSymbolType(Symbol *sym, AST *newType)
{
  AST *oldType = NULL;
  if (!newType) return 0;
  if (!sym) return 0;
  if (!gl_infer_ctypes) return 0;
  
  switch(sym->kind) {
  case SYM_VARIABLE:
  case SYM_LOCALVAR:
  case SYM_PARAMETER:
    oldType = (AST *)sym->val;
    if (oldType) {
      // FIXME: could warn here about type mismatches
      return 0;
    } else if (newType) {
        // if we had an unknown type before, the new type must be at least
        // 4 bytes wide
        sym->val = WidenType(newType);
        return 1;
    }
  default:
    break;
  }
  return 0;
}

static int
InferTypesFunccall(AST *callast)
{
    Symbol *sym;
    int changes = 0;
    AST *typelist;
    Function *func;
    AST *paramtype;
    AST *paramid;
    AST *list;


    list = callast->right;
    sym = FindCalledFuncSymbol(callast, NULL, 0);
    if (!sym || sym->kind != SYM_FUNCTION) return 0;
    
    func = (Function *)sym->val;
    typelist = func->params;
    while (list) {
        paramtype = NULL;
        sym = NULL;
        if (list->kind != AST_EXPRLIST) break;
        if (typelist) {
            paramid = typelist->left;
            typelist = typelist->right;
            if (paramid && paramid->kind == AST_IDENTIFIER) {
                sym = FindSymbol(&func->localsyms, paramid->d.string);
                if (sym && sym->kind == SYM_PARAMETER) {
                    paramtype = (AST *)sym->val;
                }
            }
        } else {
            paramid = NULL;
        }
        if (paramtype) {
            changes += InferTypesExpr(list->left, paramtype);
        } else if (paramid) {
            AST *et = ExprType(list->left);
            if (et && sym) {
                changes += SetSymbolType(sym, et);
            }
        }
        list = list->right;
    }
    return changes;
}

static int
InferTypesExpr(AST *expr, AST *expectType)
{
  int changes = 0;
  Symbol *sym;
  AST *lhsType;
  if (!expr) return 0;
  switch(expr->kind) {
  case AST_IDENTIFIER:
        lhsType = ExprType(expr);
        if (lhsType == NULL && expectType != NULL) {
          sym = LookupSymbol(expr->d.string);
          changes = SetSymbolType(sym, expectType);
      }
      return changes;
  case AST_MEMREF:
      return InferTypesExpr(expr->right, PtrType(expr->left));
  case AST_OPERATOR:
      switch (expr->d.ival) {
      case K_INCREMENT:
      case K_DECREMENT:
      case '+':
      case '-':
          if (expectType) {
              if (IsPointerType(expectType) && PointerTypeIncrement(expectType) != 1) {
                  // addition only works right on pointers of size 1
                  expectType = ast_type_generic; // force generic type
              } else if (!IsIntOrGenericType(expectType)) {
                  expectType = ast_type_generic;
              }
          }
          changes = InferTypesExpr(expr->left, expectType);
          changes += InferTypesExpr(expr->right, expectType);
          return changes;
      default:
          if (!expectType || !IsIntOrGenericType(expectType)) {
              expectType = ast_type_long;
          }
          changes = InferTypesExpr(expr->left, expectType);
          changes += InferTypesExpr(expr->right, expectType);
          return changes;
      }
#if 0
  case AST_ASSIGN:
      lhsType = ExprType(expr->left);
      if (!expectType) expectType = ExprType(expr->right);
      if (!lhsType && expectType) {
          changes += InferTypesExpr(expr->left, expectType);
      } else {
          expectType = lhsType;
      }
      changes += InferTypesExpr(expr->right, expectType);
      return changes;
#endif
  case AST_FUNCCALL:
      changes += InferTypesFunccall(expr);
      return changes;
  case AST_ADDROF:
  case AST_ABSADDROF:
      expectType = NULL; // forget what we were expecting
      // fall through
  default:
      changes = InferTypesExpr(expr->left, expectType);
      changes += InferTypesExpr(expr->right, expectType);
      return changes;
  }
}

/*
 * Fix types on parameters in the type of this function
 */
void
FixupParameterTypes(Function *pf)
{
    AST *overalltype;
    AST *a;
    AST *arg;
    AST *type;
    overalltype = pf->overalltype;
    a = overalltype->right;
    while (a) {
        arg = a->left;
        if (arg->kind == AST_IDENTIFIER) {
            type = ExprType(arg);
            if (type) {
                arg = NewAST(AST_DECLARE_VAR, type, arg);
                a->left = arg;
            }
        }
        a = a->right;
    }
}

/*
 * main entry for type checking
 * returns 0 if no changes happened, nonzero if
 * some did
 */
int
InferTypes(Module *P)
{
    Function *pf;
    int changes = 0;
    Function *savecur = curfunc;
    
    /* scan for static definitions */
    current = P;
    for (pf = P->functions; pf; pf = pf->next) {
        curfunc = pf;
        changes += InferTypesStmtList(pf->body);
        if (pf->is_static) {
            continue;
        }
        pf->is_static = 1;
        CheckForStatic(pf, pf->body);
        if (pf->is_static) {
            changes++;
            pf->force_static = 0;
        } else if (pf->force_static) {
            pf->is_static = 1;
            changes++;
        }
    }
    curfunc = savecur;
    return changes;
}

static bool
UseInternal(const char *name)
{
    Symbol *sym = FindSymbol(&systemModule->objsyms, name);
    if (sym && sym->kind == SYM_FUNCTION) {
        Function *func = (Function *)sym->val;
        MarkUsed(func, "internal");
        curfunc->is_leaf = 0; // caller is not a leaf function
        return true;
    } else {
        // don't actually error here, if we are in some modes the global modules
        // aren't compiled (e.g. C++ output)
        // ERROR(NULL, "UseInternal did not find the requested function %s", name);
    }
    return false;
}

static AST *
ConvertInternal(AST *body, const char *fname, AST *arg1, AST *arg2, AST *arg3)
{
    AST *fcall;
    AST *ident;
    if (UseInternal(fname)) {
        ASTReportInfo saveinfo;

        AstReportAs(body, &saveinfo);
        ident = AstIdentifier(fname);
        if (arg3) {
            arg3 = NewAST(AST_EXPRLIST, arg3, NULL);
        }
        if (arg2) {
            arg2 = NewAST(AST_EXPRLIST, arg2, arg3);
        }
        arg1 = NewAST(AST_EXPRLIST, arg1, arg2);
        fcall = NewAST(AST_FUNCCALL, ident, arg1);
        curfunc->is_leaf = 0; // caller is not a lear function
        AstReportDone(&saveinfo);
        return fcall;
    }
    return body;
}

static int
NonUnsignedOp(int val)
{
    switch(val) {
    case K_LEU: return K_LE;
    case K_GEU: return K_GE;
    case K_LTU: return '<';
    case K_GTU: return '>';
    default: return val;
    }
}

static int
NonFloatOp(int val)
{
    switch(val) {
    case K_FEQ: return K_EQ;
    case K_FNE: return K_NE;
    case K_FLE: return K_LE;
    case K_FGE: return K_GE;
    case K_FLT: return '<';
    case K_FGT: return '>';
    default: return val;
    }
}
        
static void
MarkUsedBody(AST *body, const char *caller)
{
    Symbol *sym;
    AST *objref;
    AST *objtype;
    Module *P;
    const char *name;
    
    if (!body) return;
    switch(body->kind) {
    case AST_FLOAT:
        ActivateFeature(FEATURE_FLOAT_USED);
        break;
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER:
        name = GetIdentifierName(body);
        sym = LookupSymbol(name);
        if (sym && sym->kind == SYM_FUNCTION) {
            Function *func = (Function *)sym->val;
            MarkUsed(func, sym->our_name);
            if (func->attributes & FUNC_ATTR_NEEDSINIT) {
                sym = LookupSymbol("__init__");
                if (sym && sym->kind == SYM_FUNCTION) {
                    func = (Function *)sym->val;
                    MarkUsed(func, "needsinit");
                }
            }
        }
        return;
    case AST_METHODREF:
        objref = body->left;
        objtype = BaseType(ExprType(objref));
        if (!objtype || objtype->kind != AST_OBJECT) {
            //not a direct object reference
        } else {
            P = GetClassPtr(objtype);
            sym = LookupSymbolInTable(&P->objsyms, GetUserIdentifierName(body->right));
            if (sym && sym->kind == SYM_FUNCTION) {
                MarkUsed((Function *)sym->val, sym->our_name);
                return;
            }
        }
        break;
    case AST_COGINIT:
        UseInternal("_coginit");
        // sanity check that enough arguments are given
        {
            AST *list;
            list = body->left;
            if (!list) {
                ERROR(body, "missing arguments to coginit/cognew");
                return;
            }
            list = list->right;
            if (!list) {
                ERROR(body, "no function or label in coginit/cognew");
                return;
            }
            if (IsSpinCoginit(body,NULL)) {
                ActivateFeature(FEATURE_MULTICOG_USED);
                if (!list->right) {
                    ERROR(body, "no stack given for coginit/cognew");
                } else {
                    list = list->right;
                    if (list->right) {
                        ERROR(body, "too many arguments to coginit/cognew");
                    }
                }
            } else if (!list->right) {
                WARNING(body, "no argument given for ASM coginit/cognew; assuming 0");
                list->right = NewAST(AST_EXPRLIST, AstInteger(0), NULL);
            } else {
                list = list->right;
                if (list->right) {
                    ERROR(body, "too many arguments to coginit/cognew");
                }
            }
        }
        break;
    case AST_LOOKUP:
        UseInternal("_lookup");
        break;
    case AST_LOOKDOWN:
        UseInternal("_lookdown");
        break;
    case AST_ROUND:
        if (!IsConstExpr(body->left)) {
            *body = *ConvertInternal(body, "_float_round", body->left, NULL, NULL);
        }
        break;
    case AST_TRUNC:
        if (!IsConstExpr(body->left)) {
            *body = *ConvertInternal(body, "_float_trunc", body->left, NULL, NULL);
        }
        break;
    case AST_TOFLOAT:
        if (!IsConstExpr(body->left)) {
            *body = *ConvertInternal(body, "_float_fromint", body->left, NULL, NULL);
        }
        break;
    case AST_OPERATOR:
        switch (body->d.ival) {
        case K_SQRT:
            *body = *ConvertInternal(body, "_sqrt", body->right, NULL, NULL);
            break;
        case K_FSQRT:
            *body = *ConvertInternal(body, "_float_sqrt", body->right, NULL, NULL);
            break;
        case K_FABS:
            *body = *ConvertInternal(body, "_float_abs", body->right, NULL, NULL);
            break;
        case K_ONES_COUNT:
            *body = *ConvertInternal(body, "_ones", body->right, NULL, NULL);
            break;
        case K_SCAS:
            *body = *ConvertInternal(body, "_scas", body->left, body->right, NULL);
            break;
        case K_QEXP:
            *body = *ConvertInternal(body, "_qexp", body->right, NULL, NULL);
            break;
        case K_QLOG:
            *body = *ConvertInternal(body, "_qlog", body->right, NULL, NULL);
            break;
        case K_FRAC64:
            *body = *ConvertInternal(body, "_qfrac", body->left, body->right, NULL);
            break;
        case '?':
            {
                AST *var;
                AST *fcall;
                if (body->left) {
                    var = body->left;
                    fcall = ConvertInternal(body, "_lfsr_forward", var, NULL, NULL);
                } else {
                    var = body->right;
                    fcall = ConvertInternal(body, "_lfsr_backward", var, NULL, NULL);
                }
                if (body != fcall) {
                    // only do this if ConvertInternal actually changed us
                    *body = *AstAssign(var, fcall);
                }
            }
            break;
            /* on some platforms, some unsigned operations are done in software */
            /* NOTE: ConvertInternal will return the original AST if the specified function is not found,
               so we can easily disable these conversions by deleting them from the appropriate file in sys/ */
        case K_UNS_DIV:
            *body = *ConvertInternal(body, "_unsigned_div", body->left, body->right, NULL);
            break;
        case K_UNS_MOD:
            *body = *ConvertInternal(body, "_unsigned_mod", body->left, body->right, NULL);
            break;
        case K_UNS_HIGHMULT:
            *body = *ConvertInternal(body, "_unsigned_himul", body->left, body->right, NULL);
            break;
        case K_LTU:
        case K_GTU:
        case K_LEU:
        case K_GEU:
            if (gl_output == OUTPUT_BYTECODE && gl_interp_kind == INTERP_KIND_P1ROM) {
                // convert to e.g. _unsigned_cmp(left, right) < 0
                AST *replace = ConvertInternal(body, "_unsigned_cmp", body->left, body->right, NULL);
                if (replace != body) {
                    body->d.ival = NonUnsignedOp(body->d.ival);
                    body->left = replace;
                    body->right = AstInteger(0);
                }
            }
            break;
        case K_FLT:
        case K_FGT:
        case K_FLE:
        case K_FGE:
        case K_FEQ:
        case K_FNE:
            if (1) {
                // convert to e.g. _float_cmp(left, right, 1) < 0
                AST *defaultval, *replace;
                int op = body->d.ival;
                if (op == K_FGE || op == K_FGT) {
                    defaultval = AstInteger(-1); // return -1 for unordered
                } else {
                    defaultval = AstInteger(1); // return +1 for unordered
                }
                replace = ConvertInternal(body, "_float_cmp", body->left, body->right, defaultval);
                if (replace != body) {
                    body->d.ival = NonFloatOp(body->d.ival);
                    body->left = replace;
                    body->right = AstInteger(0);
                }
            }
            break;
        case K_FADD:
            *body = *ConvertInternal(body, "_float_add", body->left, body->right, NULL);
            break;
        case K_FSUB:
            *body = *ConvertInternal(body, "_float_sub", body->left, body->right, NULL);
            break;
        case K_FMUL:
            *body = *ConvertInternal(body, "_float_mul", body->left, body->right, NULL);
            break;
        case K_FDIV:
            *body = *ConvertInternal(body, "_float_div", body->left, body->right, NULL);
            break;
        default:
            break;
        }
        break;
    case AST_ADDROF:
    case AST_ABSADDROF:
        if (IsFunctionType(ExprType(body->left))) {
            // we will need method pointers
            UseInternal("__call_methodptr");
        }
        break;
    case AST_SETJMP:
    case AST_THROW:
    case AST_CATCH:
    case AST_TRYENV:
    case AST_CATCHRESULT:
        gl_features_used |= FEATURE_LONGJMP_USED;
        break;
    case AST_GOSUB:
        gl_features_used |= FEATURE_GOSUB_USED;
        UseInternal("__gosub_helper");
        break;
    default:
        break;
    }
    MarkUsedBody(body->left, caller);
    MarkUsedBody(body->right, caller);
}

#define CALLSITES_MANY 8

void
MarkUsed(Function *f, const char *caller)
{
    Module *oldcurrent;
    Function *oldfunc;
    
    if (!f || f->callSites > CALLSITES_MANY) {
        return;
    }
    if (f->attributes & FUNC_ATTR_COMPLEXIO) {
        ActivateFeature(FEATURE_COMPLEXIO);
    }
    f->callSites++;
    if (f->callSites == 1) {
        f->caller = caller;
    }
    oldcurrent = current;
    oldfunc = curfunc;
    current = f->module;
    curfunc = f;
    MarkUsedBody(f->body, caller);
    current = oldcurrent;
    curfunc = oldfunc;
}

void
SetFunctionReturnType(Function *f, AST *typ)
{
    if (typ && !f->overalltype->left) {
        if (typ == ast_type_byte || typ == ast_type_word) {
            typ = ast_type_long;
        }
        if (typ && typ->kind == AST_TUPLE_TYPE) {
            f->overalltype->left = typ;
            if (f->numresults > 1) {
                if (f->numresults != AstListLen(typ)) {
                    ERROR(NULL, "inconsistent return values from function %s", f->name);
                }
            }
            f->numresults = typ->d.ival;
        } else if (gl_infer_ctypes) {
            f->overalltype->left = typ;
        } else {
            f->overalltype->left = ast_type_generic;
        }
    }
    if (typ && typ->kind == AST_TUPLE_TYPE) {
        int len = AstListLen(typ);
        if (f->numresults <= 1) {
            f->numresults = len;
        }
        else if (f->numresults != len) {
            ERROR(NULL, "inconsistent return values from function %s", f->name);
        }
    }
}

/*
 * check to see if function ref may be called from AST body
 * also clears the is_leaf flag on the function if we see any
 * calls within it
 */
bool
IsCalledFrom(Function *ref, AST *body, int visitRef)
{
    Module *oldState;
    Function *oldCurFunc;
    Symbol *sym;
    Function *func;
    bool result;
    
    if (!body) return false;
    switch(body->kind) {
    case AST_FUNCCALL:
        ref->is_leaf = 0;
        sym = FindCalledFuncSymbol(body, NULL, 0);
        if (!sym) return false;
        if (sym->kind != SYM_FUNCTION) return false;
        func = (Function *)sym->val;
        if (ref == func) return true;
        if (func->visitFlag == visitRef) {
            // we've been here before
            return false;
        }
        func->visitFlag = visitRef;
        oldState = current;
        oldCurFunc = curfunc;
        current = func->module;
        curfunc = func;
        result = IsCalledFrom(ref, func->body, visitRef);
        current = oldState;
        curfunc = oldCurFunc;
        return result;
    case AST_COGINIT:
    case AST_LOOKUP:
    case AST_LOOKDOWN:
        ref->is_leaf = 0;
        break;
    case AST_OPERATOR:
        switch (body->d.ival) {
        case K_SQRT:
        case '?':
            // can produce an internal function call
            ref->is_leaf = 0;
            break;
        default:
            break;
        }
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
    f->is_leaf = 1; // possibly a leaf function
    f->is_recursive = IsCalledFrom(f, f->body, visitPass);
}

void
DeclareFunctionTemplate(Module *P, AST *templ)
{
    Symbol *sym;
    AST *ident;
    const char *name_user, *name_internal;

    /* templ->left is the list of type names */
    /* templ->right->left is the function type signature */
    /* templ->right->right->left is the name */
    ident = templ->right->right->left;
    if (ident->kind == AST_LOCAL_IDENTIFIER) {
      name_internal = ident->left->d.string;
      name_user = ident->right->d.string;
    } else if (ident->kind == AST_IDENTIFIER) {
      name_internal = name_user = ident->d.string;
    } else {
        ERROR(templ, "Internal error, no template name found");
        return;
    }
    /* check for existing definition */
    sym = FindSymbol(&P->objsyms, name_internal);
    if (sym) {
        AST *olddef = (AST *)sym->def;
        ERROR(templ, "Redefining symbol %s", name_user);
        if (olddef) {
            ERROR(olddef, "...previous definition was here");
        }
    }
    /* create */
    sym = AddSymbol(&P->objsyms, name_internal, SYM_TEMPLATE, (void *)templ, name_user);
}

static char *
concatstr(const char *base, const char *suffix)
{
  int len = strlen(base) + strlen(suffix) + 1;
  char *next = (char *)malloc(len);
  strcpy(next, base);
  strcat(next, suffix);
  return next;
}

static const char *
appendType(const char *base, AST *typ)
{
  char buf[32];
  Module *P;
  if (!typ) return base;
  switch (typ->kind) {
  case AST_MODIFIER_CONST:
  case AST_MODIFIER_VOLATILE:
    return appendType(base, typ->left);
  case AST_INTTYPE:
    sprintf(buf, "i%" PRId64, EvalConstExpr(typ->left));
    return concatstr(base, buf);
  case AST_UNSIGNEDTYPE:
    sprintf(buf, "u%" PRId64, EvalConstExpr(typ->left));
    return concatstr(base, buf);
  case AST_FLOATTYPE:
    sprintf(buf, "f%" PRId64, EvalConstExpr(typ->left));
    return concatstr(base, buf);
  case AST_GENERICTYPE:
    return concatstr(base, "g");
  case AST_REFTYPE:
  case AST_PTRTYPE:
    base = concatstr(base, "p");
    return appendType(base, typ->left);
  case AST_FUNCTYPE:
    base = concatstr(base, "m");
    return appendType(base, typ->left);
  case AST_ARRAYTYPE:
    sprintf(buf, "a%" PRId64, EvalConstExpr(typ->right));
    base = concatstr(base, buf);
    return appendType(base, typ->left);
  case AST_OBJECT:
    P = GetClassPtr(typ);
    sprintf(buf, "x%ld", (long)strlen(P->classname));
    base = concatstr(base, buf);
    base = concatstr(base, P->classname);
    return base;
  default:
    ERROR(typ, "do not know how to express type");
    return base;
  }
}

// determine the function name for a template
// templpairs is a list each of whose elements contains
// an AST_SEQUENCE with left=template name right=template type

const char *TemplateFuncName(AST *templpairs, const char *orig_base)
{
  AST *ast, *typ;
  const char *base = concatstr(orig_base, "__");
  while (templpairs) {
    ast = templpairs->left;
    templpairs = templpairs->right;
    typ = ast->right;
    base = appendType(base, typ);
  }
  return base;
}

static AST *matchType(AST *decl, AST *use, AST *var)
{
  if (decl->kind == AST_REFTYPE) {
      decl = decl->left;
  }
  if (decl->kind == AST_IDENTIFIER) {
    return use;
  }
  if (decl->kind == use->kind) {
    return matchType(decl->left, use->left, var);
  }
  ERROR(decl, "Unable to match template variable %s", GetUserIdentifierName(var));
  return ast_type_generic;
}

// try to figure out the types required to instantiate a template
// returns a list of pairs containing (template name, type)
static AST *deduceTemplateTypes(AST *templateVars, AST *functype, AST *call)
{
  AST *funcparams = functype->right;
  AST *callparams = call->right;
  AST *typ;
  AST *decltyp;
  AST *var;
  AST *ast, *templVar;
  AST *q;
  AST *pairs = NULL;
  AST *thispair;
  const char *templName;
  
  while (funcparams && callparams) {
    var = funcparams->left;
    if (var && var->kind == AST_DECLARE_VAR) {
      decltyp = var->left;
      // check for use of one of the template variables
      for (ast = templateVars; ast; ast = ast->right) {
	templVar = ast->left;
	templName = GetIdentifierName(templVar);
	if (AstUses(decltyp, templVar)) {
	  typ = ExprType(callparams->left);
	  typ = matchType(decltyp, typ, templVar);
	  // now look for it in the list of pairs
	  for (q = pairs; q; q = q->right) {
	    thispair = q->left;
	    if (AstMatch(thispair->left, templVar)) {
	      // already found; verify the type
	      if (!CompatibleTypes(thispair->right, typ)) {
                  const char *name = NULL;
                  if (call->kind == AST_FUNCCALL) {
                      name = GetUserIdentifierName(call->left);
                  }
                  if (!name) {
                      name = "function call";
                  }
                  ERROR(callparams, "Inconsistent types for template parameter %s in %s", templName, name);
              }
	      break;
	    }
	  }
	  if (!q) {
	    thispair = NewAST(AST_SEQUENCE, DupAST(templVar), typ);
	    pairs = AddToList(pairs, NewAST(AST_LISTHOLDER, thispair, NULL));
	  }
	  break;
	}
      }
    }
    funcparams = funcparams->right;
    callparams = callparams->right;
  }
  return pairs;    
}

static AST *
fixupFunctype(AST *pairs, AST *orig)
{
  AST *thispair;
  AST *t;
  if (!orig) return NULL;
  switch (orig->kind) {
  case AST_IDENTIFIER:
    for (t = pairs; t; t = t->right)
      {
	thispair = t->left;
	if (AstMatch(thispair->left, orig)) {
	  return thispair->right;
	}
      }
    // identifier was not found in the template list, so just return it
    return orig;
  default:
    t = orig;
    t->left = fixupFunctype(pairs, orig->left);
    t->right = fixupFunctype(pairs, orig->right);
    return t;
  }
}

/* optimize template instantiations by evaluating constant "if" expressions */
/* this is important for removing stuff protected by _SameTypes() and _HasMethod();
 * we don't want later code to see syntax errors due to missing methods
 */

static AST *
ReduceTemplateConstants(AST *body)
{
    AST *cond;
    if (!body) return body;
    switch (body->kind) {
    case AST_IF:
    case AST_CONDRESULT:
        body->left = ReduceTemplateConstants(body->left);
        body->right = ReduceTemplateConstants(body->right);
        cond = body->left;
        if (IsConstExpr(cond)) {
            AST *ifpart = body->right->left;
            AST *elsepart = body->right->right;
            if (EvalConstExpr(cond)) {
                return ifpart;
            } else {
                return elsepart;
            }
        }
        break;
    default:
        body->left = ReduceTemplateConstants(body->left);
        body->right = ReduceTemplateConstants(body->right);
        break;
    }
    return body;
}

/*
 * instantiate a template function call
 * types are deduced from the types in the provided function call
 * returns an identifier for the specialized function
 */

AST *
InstantiateTemplateFunction(Module *P, AST *templ, AST *call)
{
  AST *templateVars = templ->left;
  AST *ident;
  AST *functype;
  AST *body;
  AST *pairs;
  AST *funcblock;
  const char *name;
  Symbol *sym;
  Module *savecur = current;
  
  templateVars = templ->left;
  templ = templ->right;
  functype = templ->left;
  templ = templ->right;
  ident = templ->left;
  templ = templ->right;
  body = templ->left;

  // assign types for each of the items in templateVars,
  // based on the types passed in the parameters
  pairs = deduceTemplateTypes(templateVars, functype, call);
  name = GetIdentifierName(ident);
  name = TemplateFuncName(pairs, name);

  ident = AstIdentifier(name);
  sym = FindSymbol(&P->objsyms, name);
  if (!sym) {
    Function *fdef;
    current = P;
    functype = fixupFunctype(pairs, DupASTTypeSafe(functype));
    body = fixupFunctype(pairs, DupASTTypeSafe(body));
    body = ReduceTemplateConstants(body);
    funcblock = DeclareTypedFunction(P, functype, ident, 1, body, NULL, NULL);
    fdef = doDeclareFunction(funcblock);
    if (fdef) {
        fdef->is_static = 1;
        CheckForStatic(fdef, fdef->body);
        ProcessOneFunc(fdef);
    }
  }
  current = savecur;
  return ident;
}

//
// return true if a function should be removed
//
bool
ShouldSkipFunction(Function *f)
{
    if (gl_output == OUTPUT_COGSPIN)
        return false;
    if (0 != f->callSites)
        return false;
    if (f->cog_task)
        return false; // used in another cog
    if (f->used_as_ptr)
        return false; // used as a pointer

    // stuff in the "system" module should be skipped
    if (IsSystemModule(f->module))
        return true;
    if (0 == (gl_optimize_flags & OPT_REMOVE_UNUSED_FUNCS))
        return false;
    return true;
}

//
// the size of local variables
//
static int AddSize(Symbol *sym, void *arg)
{
    int *ptr = (int *)arg;
    int size;
    switch (sym->kind) {
    case SYM_LOCALVAR:
    case SYM_TEMPVAR:
    case SYM_PARAMETER:
        size = TypeSize((AST *)sym->val);
        size = (size + LONG_SIZE - 1) & ~(LONG_SIZE - 1);
        *ptr += size;
        break;
    default:
        break;
    }
    return 1;
}

int FuncLocalSize(Function *func)
{
    int size = LONG_SIZE * (func->numresults);
    // iterate over local variables, incrementing the size
    IterateOverSymbols(&func->localsyms, AddSize, (void *)&size);
    return size;
}
