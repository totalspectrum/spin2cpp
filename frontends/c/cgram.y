/*
 * C compiler parser
 * Copyright (c) 2011-2020 Total Spectrum Software Inc.
 * See the file COPYING for terms of use.
 */

%pure-parser

%{
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdlib.h>
#include "frontends/common.h"
#include "frontends/lexer.h"

#define CGRAMYYSTYPE AST*
#undef  YYSTYPE
#define YYSTYPE CGRAMYYSTYPE
#define IN_DAT 1
    
/* special flag */
    AST *ast_type_c_signed = NULL;
    AST *ast_type_c_long = NULL;
    
/* Yacc functions */
    void cgramyyerror(const char *);
    int cgramyylex();

    extern int gl_errors;

    extern AST *last_ast;
    extern AST *CommentedListHolder(AST *); // in spin.y

#define YYERROR_VERBOSE 1

extern int allow_type_names;
    
static void
DisallowTypeNames()
{
    allow_type_names = 0;
}

static void
AllowTypeNames()
{
    allow_type_names = 1;
}

static AST *
MakeSigned(AST *type, int isSigned)
{
    if (!type) return type;
    if (type->kind == AST_INTTYPE) {
        if (isSigned) return type;
        return NewAST(AST_UNSIGNEDTYPE, type->left, NULL);
    }
    if (type->kind == AST_UNSIGNEDTYPE) {
        if (!isSigned) return type;
        return NewAST(AST_INTTYPE, type->left, NULL);
    }
    type->left = MakeSigned(type->left, isSigned);
    return type;
}
static AST *
LengthenType(AST *type)
{
    if (!type) {
        return ast_type_c_long;
    }
    if (type == ast_type_c_long) {
        // "long long" -> 8 byte type
        return NewAST(AST_INTTYPE, AstInteger(8), NULL);
    }
    if (type->kind == AST_FLOATTYPE) {
        // "long double" -> 8 byte double
        return NewAST(AST_FLOATTYPE, AstInteger(8), NULL);
    }
    // otherwise make sure it is 4 bytes long
    return NewAST(type->kind, AstInteger(4), NULL);
}

static AST *
LabelName(AST *x)
{
    const char *orig_name = GetUserIdentifierName(x);
    char *new_name;

    if (!orig_name) return x;
    new_name = calloc(strlen(orig_name) + 8, 1);
    strcpy(new_name, "label:");
    strcat(new_name, orig_name);
    return AstIdentifier(new_name);
}

static AST *
C_ModifySignedUnsigned(AST *modifier, AST *type)
{
    if (modifier == ast_type_c_signed) {
        type = MakeSigned(type, 1);
    } else if (modifier == ast_type_unsigned_long) {
        type = MakeSigned(type, 0);
    }
    // we need to be able to distinguish between
    // "long long" and "long int"
    if (modifier == ast_type_c_long) {
        type = LengthenType(type);
    }
    return type;
}

static AST *
CombinePointer(AST *ptr, AST *type)
{
    AST *q = ptr;
    while (q && q->left) {
        q = q->left;
    }
    if (q) {
        q->left = type;
    }
    return ptr;
}

static AST *MergePrefix(AST *prefix, AST *first)
{
    if (prefix) {
        // make sure we migrate STATIC to the front of the list
        if (first && first->kind == AST_STATIC) {
            first->left = AddToLeftList(prefix, first->left);
        } else {
            first = AddToLeftList(prefix, first);
        }
    }
    return first;
}

static AST *
CombineTypes(AST *first, AST *second, AST **identifier)
{
    AST *expr, *ident;
    AST *prefix = NULL;
    
    if (second && second->kind == AST_COMMENT) {
        second = NULL;
    }
    if (first && first->kind == AST_COMMENT) {
        first = NULL;
    }
    if (!second) {
        return first;
    }
    if (first && (first->kind == AST_STATIC || first->kind == AST_TYPEDEF || first->kind == AST_EXTERN)) {
        prefix = DupAST(first);
        first = first->left;
        prefix->left = NULL;
    }
    switch (second->kind) {
    case AST_DECLARE_VAR:
        first = CombineTypes(first, second->left, identifier);
        first = CombineTypes(first, second->right, identifier);
        return MergePrefix(prefix, first);
    case AST_SYMBOL:
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER:
        if (identifier) {
            *identifier = second;
        }
        return MergePrefix(prefix, first);
    case AST_ARRAYDECL:
        first = NewAST(AST_ARRAYTYPE, first, second->right);
        return MergePrefix(prefix, CombineTypes(first, second->left, identifier));
    case AST_REFTYPE:
    case AST_PTRTYPE:
        first = NewAST(second->kind, first, NULL);
        second = CombineTypes(first, second->left, identifier);
        return MergePrefix(prefix, second);
        
    case AST_FUNCTYPE:
        second->left = CombineTypes(first, second->left, identifier);
        return MergePrefix(prefix, second);
    case AST_ASSIGN:
        expr = second->right;
        first = CombineTypes(first, second->left, &ident);
        ident = AstAssign(ident, expr);
        if (identifier) {
            *identifier = ident;
        }
        return MergePrefix(prefix, first);
    case AST_MODIFIER_CONST:
    case AST_MODIFIER_VOLATILE:
        expr = NewAST(second->kind, NULL, NULL);
        second = MergePrefix(prefix, CombineTypes(first, second->left, identifier));
        expr->left = second;
        return expr;
    case AST_BITFIELD:
        // first == type
        // second == BITFIELD(ident, size)
        // -> BITFIELD(type, size)
        if (identifier) {
            *identifier = second->left;
        }
        second->left = first;
        return second;
    default:
        if (!first) {
            return MergePrefix(prefix, second);
        }
        ERROR(first, "Internal error: don't know how to combine types");
        return first;
    }
}

static AST *
DeclareStatics(Module *P, AST *basetype, AST *decllist)
{
    AST *ident;
    AST *decl;
    AST *nameAst;
    AST *globalname;
    AST *results = NULL;
    const char *nameString;
    int needs_initializer = 0;
    int has_initializer;
    
    // ignore static declarations like
    //   static int blah[]
    if (basetype && basetype->kind == AST_ARRAYTYPE && !basetype->right) {
        needs_initializer = 1;
    }
    // go through the identifier list
    while (decllist) {
        has_initializer = 0;
        if (decllist->kind == AST_LISTHOLDER) {
            decl = decllist->left;
            decllist = decllist->right;
        } else {
            decl = decllist;
            decllist = NULL;
        }
        if (decl->kind == AST_ASSIGN) {
            ident = decl->left;
            has_initializer = 1;
        } else {
            ident = decl;
        }
        if (ident->kind == AST_ARRAYDECL) {
            nameAst = ident->left;
        } else {
            nameAst = ident;
        }
        if (needs_initializer && !has_initializer) {
            continue;
        }
        if (nameAst->kind == AST_LOCAL_IDENTIFIER) {
            // we have to be very careful here; a new static declaration
            // for an identifier needs to override an existing static in
            // an outer scope
            Symbol *sym;
            const char *idstr = GetUserIdentifierName(nameAst);
            // use FindSymbol to look only in current tables, not enclosing
            // ones
            sym = FindSymbol(currentTypes, idstr);
            if (!sym) {
                *nameAst = *nameAst->right; // create a new local
            }
        }
        if (nameAst->kind == AST_LOCAL_IDENTIFIER) {
            DeclareOneGlobalVar(P, decl, basetype, IN_DAT);
        } else {
            // OK, "nameAst" is the name we want it to be known as inside
            // the function, but we will want to create a global variable
            // with a new name

            nameString = GetIdentifierName(nameAst);
            globalname = AstTempIdentifier(nameString);
            EnterLocalAlias(currentTypes, globalname, nameString);
            // and enter a new global definition
            *nameAst = *globalname;
            DeclareOneGlobalVar(P, decl, basetype, IN_DAT);
        }
    }
    return results;
}

static AST *
MultipleDeclareVar(AST *first, AST *second)
{
    AST *ident, *type;
    AST *stmtlist = NULL;
    AST *item;
    
    while (second) {
        if (second->kind != AST_LISTHOLDER) {
            ERROR(second, "internal error in createVarDeclarations: expected listholder");
            return stmtlist;
        }
        item = second->left;
        second = second->right;
        type = CombineTypes(first, item, &ident);
        if (type && type->kind == AST_STATIC) {
            stmtlist = AddToList(stmtlist, DeclareStatics(current, type->left, ident));
        } else if (type && type->kind == AST_EXTERN) {
            /* do nothing */
        } else {
            ident = NewAST(AST_DECLARE_VAR, type, ident);
            stmtlist = AddToList(stmtlist, NewAST(AST_STMTLIST, ident, NULL));
        }
    }
    return stmtlist;
}

AST *
SingleDeclareVar(AST *decl_spec, AST *declarator)
{
    AST *type, *ident;

    ident = NULL;
    type = CombineTypes(decl_spec, declarator, &ident);
    if (type && type->kind == AST_EXTERN) {
        // just ignore EXTERN declarations
        return NULL;
    }
    if (!ident && !type) {
        return NULL;
    }
    return NewAST(AST_DECLARE_VAR, type, ident);
}

static void
DeclareCGlobalVariables(AST *slist)
{
    AST *temp;
    int inDat;

    if (!current || IsTopLevel(current) || !strcmp(current->classname, "_system_")) {
        inDat = 1;
    } else {
//        inDat = 0;
        inDat = 1;
    }
    if (slist && slist->kind == AST_DECLARE_VAR) {
        DeclareTypedGlobalVariables(slist, inDat);
        return;
    }
    while (slist) {
        if (slist->kind != AST_STMTLIST) {
            ERROR(slist, "internal error in DeclareCGlobalVars");
        }
        temp = slist->left;
        DeclareTypedGlobalVariables(temp, inDat);
        slist = slist->right;
    }
}

/* process a parameter list and fix it up as necessary */
static AST *
ProcessParamList(AST *list)
{
    AST *entry, *type;
    int count = 0;
    AST *orig_list = list;

    if (list->kind == AST_EXPRLIST) {
        while (list) {
            entry = list->left;
            list->left = NewAST(AST_DECLARE_VAR, ast_type_long, entry);
            list->kind = AST_LISTHOLDER;
            list = list->right;
        }
        list = orig_list;
    }
    while (list) {
        entry = list->left;
        list = list->right;
        if (entry == ast_type_void) {
            if (list || count) {
                SYNTAX_ERROR("void should appear alone in a parameter list");
            }
            return NULL;
        }
        if (entry && entry->kind == AST_DECLARE_VAR) {
            type = entry->left;
            while (type->kind == AST_MODIFIER_CONST || type->kind == AST_MODIFIER_VOLATILE) {
                type = type->left;
            }
            if (type->kind == AST_ARRAYTYPE) {
                type->kind = AST_PTRTYPE;
            }
        }
        count++;
    }
    return orig_list;
}

/* make sure a statement is embedded in a statement list */
static AST *
ForceStatementList(AST *stmt)
{
    if (stmt && stmt->kind != AST_STMTLIST) {
        return NewAST(AST_STMTLIST, stmt, NULL);
    }
    return stmt;
}

static AST *
AddEnumerators(AST *identifier, AST *enumlist)
{
    AST *resetZero = NewAST(AST_ENUMSET, AstInteger(0), NULL);
    Module *P;
    enumlist = NewAST(AST_LISTHOLDER, resetZero, enumlist);

    if (current->curLanguage == LANG_CFAMILY_C) {
        // enum symbols in C get declared with global scope
        P = GetTopLevelModule();
        if (P->curLanguage != LANG_CFAMILY_C) {
            // handle the case where a C file is included as a module
            // in another language
            P = current;
        }
    } else {
        // in C++ enums get declared in the context of the class
        // they're declared in
        P = current;
    }
    // we have to process the enumerators now so that they may be used
    // in struct definitions and such
    P->conblock = AddToList(P->conblock, enumlist);
    DeclareConstants(P, &P->conblock);
    return ast_type_long;
}

static void
DeclareCMemberVariables(Module *P, AST *astlist, int is_union)
{
    AST *idlist, *typ;
    AST *ident;
    AST *ast;
    int bitfield_offset = 0;
    int bitfield_size = 0;
    int max_bitfield_size = 0;
    AST *bitfield_ident = 0;
    int is_private = 0;
    AST *last_pos = 0;
    
    if (!astlist) return;
    if (astlist->kind != AST_STMTLIST) {
        ERROR(astlist, "Internal error, expected stmt list");
        return;
    }
    if (is_union) {
        P->isUnion = 1;
    }
    while (astlist) {
        ast = astlist->left;
        astlist = astlist->right;
        if (ast->kind == AST_FUNCDECL) {
            AST *type;
            AST *ident;
            AST *body;
            int is_public = 1;
            AST *list = ast->left;
            if (list->kind != AST_LISTHOLDER) {
                ERROR(list, "malformed func decl");
                return;
            }
            type = list->left; list = list->right;
            ident = list->left; list = list->right;
            body = list->left;
            DeclareTypedFunction(P, type, ident, is_public, body, NULL, NULL);
            continue;
        }
        if (ast->kind == AST_PUBFUNC) {
            is_private = 0;
            continue;
        }
        if (ast->kind == AST_PRIFUNC) {
            is_private = 1;
            continue;
        }
        if (ast->kind != AST_DECLARE_VAR) {
            ERROR(ast, "internal error, not DECLARE_VAR");
            return;
        }
        idlist = ast->right;
        typ = ast->left;
        if (idlist->kind == AST_LISTHOLDER) {
            if (typ->kind == AST_BITFIELD) {
                ERROR(typ, "Internal error, bitfield in a list");
                typ = typ->left;
            }
            while (idlist) {
                ident = idlist->left;
                // not in a bitfield
                max_bitfield_size = bitfield_size = bitfield_offset = 0;
                MaybeDeclareMemberVar(P, ident, typ, is_private, NORMAL_VAR);
                idlist = idlist->right;
            }
        } else {
            ident = idlist;
            if (typ->kind == AST_BITFIELD) {
                AST *bfield_ast = typ->right;
                AST *bfield_typ = typ->left;
                AST *bfield_access;
                AST *bfield_list = 0;
                int tsize;
                int bsize = EvalConstExpr(bfield_ast);
                tsize = TypeSize(bfield_typ) * 8;
                if (max_bitfield_size == 0 || max_bitfield_size != tsize || bitfield_offset + bsize > max_bitfield_size) {
                    // start a new bitfield
                    max_bitfield_size = tsize;
                    bitfield_offset = 0;
                    bitfield_ident = AstTempIdentifier("__bitfield_");
                    last_pos = MaybeDeclareMemberVar(P, bitfield_ident, bfield_typ, is_private, HIDDEN_VAR);
                }
                if (bsize > max_bitfield_size) {
                    ERROR(bfield_ast, "bitfield size %d is greater than type size %d",
                          bsize, max_bitfield_size);
                    bsize = max_bitfield_size;
                }
                if (bsize < 0) {
                    bsize = 1;
                }
                bfield_access = NewAST(AST_RANGE, AstInteger(bitfield_offset + bsize - 1), AstInteger(bitfield_offset));
                bfield_access = NewAST(AST_RANGEREF, bitfield_ident, bfield_access);
                bfield_access = NewAST(AST_CAST, bfield_typ, bfield_access);
                DeclareMemberAlias(P, ident, bfield_access);
                bfield_list = NewAST(AST_DECLARE_BITFIELD, bfield_access, ident);
                bfield_list = NewAST(AST_LISTHOLDER, bfield_list, NULL);
                P->pendingvarblock = ListInsertBefore(P->pendingvarblock, last_pos, bfield_list);
                bitfield_offset += bsize;
            } else {
                // not in a bitfield
                max_bitfield_size = bitfield_size = bitfield_offset = 0;
                MaybeDeclareMemberVar(P, ident, typ, is_private, NORMAL_VAR);
            }
        }
    }

}

static void
AddStructBody(Module *C, AST *body)
{
    if (body) {
        int is_union = C->isUnion;
        DeclareCMemberVariables(C, body, is_union);
        DeclareMemberVariables(C);
    }
}

// make a new struct
// skind is either AST_STRUCT or AST_UNION
// identifier is NULL or is a struct tag
// body is the contents of the struct, or NULL
// if no struct tag is given, we use one generated from the
// file name and line number
//
static AST *
MakeNewStruct(Module *Parent, AST *skind, AST *identifier, AST *body)
{
    int is_union;
    int is_class;
    const char *name;
    const char *classname;
    char *typname;
    Module *C;
    Symbol *sym;
    AST *class_type;
    
    if (identifier && identifier->kind == AST_LOCAL_IDENTIFIER) {
        identifier = identifier->right;
    }
    if (Parent->mainLanguage == LANG_CFAMILY_C) {
        // structs inside structs get declared with global scope
        Module *Q = GetTopLevelModule();
        if (Q->mainLanguage == LANG_CFAMILY_C) {
            Parent = Q;
        }
    }
    /* attempt to better handle class inside class */
    if (0 && current && !IsTopLevel(current)) {
        /* this causes problems with struct references inside structs */
        classname = current->classname;
    } else {
        classname = "";
    }
    if (skind->kind == AST_STRUCT) {
        is_union = 0;
        is_class = skind->d.ival != 0;
    } else if (skind->kind == AST_UNION) {
        is_union = 1;
        is_class = 0;
    } else {
        ERROR(skind, "internal error: not struct or union");
        return NULL;
    }
    if (!identifier) {
        // use file name and line number
        char buf[128];
        unsigned int hash = RawSymbolHash(current->Lptr->fileName);
        sprintf(buf, "_anon_%08x%08x", hash, current->Lptr->lineCounter);
        identifier = AstIdentifier(strdup(buf));
    }
    if (!IsIdentifier(identifier)) {
        ERROR(identifier, "internal error: bad struct def");
        return NULL;
    }
    name = GetIdentifierName(identifier);
    typname = (char *)malloc(strlen(name)+strlen(classname)+16);
    strcpy(typname, classname);
    strcat(typname, "__struct_");
    strcat(typname, name);

    /* see if there is already a type with that name */
    sym = LookupSymbolInTable(currentTypes, typname);
    if (!sym) {
        sym = LookupSymbolInTable(&Parent->objsyms, typname);
    }
    if (sym && sym->kind == SYM_TYPEDEF) {
        class_type = (AST *)sym->val;
        if (!IsClassType(class_type)) {
            SYNTAX_ERROR("%s is not a class", typname);
            return NULL;
        }
        C = (Module *)class_type->d.ptr;
        if (C->isUnion != is_union) {
            SYNTAX_ERROR("Inconsistent use of union/struct for %s", typname);
        }
        C->Lptr = current->Lptr;
    } else {
        if (body && body->kind == AST_STRING) {
            class_type = NewAbstractObject(AstIdentifier(typname), body);
            Parent->objblock = AddToList(Parent->objblock, class_type);
            body = NULL;
            C = NULL;
        } else {
            C = NewModule(typname, LANG_CFAMILY_C);
            C->defaultPrivate = is_class;
            C->Lptr = current->Lptr;
            C->isUnion = is_union;
            class_type = NewAbstractObject(AstIdentifier(typname), NULL);
            class_type->d.ptr = C;
            AddSymbol(currentTypes, typname, SYM_TYPEDEF, class_type, NULL);
            AddSymbol(&Parent->objsyms, typname, SYM_TYPEDEF, class_type, NULL);
            AddSubClass(Parent, C);
        }
    }
    AddStructBody(C, body);
    return class_type;
}

//
// utility function: find a variable in a declaration list
//
static AST *
FindDeclInList(AST *param, AST *decl_list)
{
    AST *decl;
    AST *ident;
    while (decl_list) {
        if (decl_list->kind != AST_STMTLIST) {
            ERROR(decl_list, "Internal error, badly formed declaration list");
            return NULL;
        }
        decl = decl_list->left;
        if (decl->kind == AST_DECLARE_VAR) {
            ident = decl->right;
            if (AstUses(ident, param)) {
                return DupAST(decl);
            }
        }
        decl_list = decl_list->right;
    }
    return NULL;
}

//
// convert an old style declaration like:
//   int strlen(x) char *x; { ... }
// into
//   int strlen(char *x) { ... }
//
// actually this part just finds the declarations
//
AST *
MergeOldStyleDeclarationList(AST *orig_funcdecl, AST *decl_list)
{
    AST *funcdecl = orig_funcdecl;
    AST *param_list;
    AST *param;
    if (!funcdecl) return NULL;
    while (funcdecl && funcdecl->kind == AST_PTRTYPE) {
        funcdecl = funcdecl->left;
    }
    if (funcdecl->kind != AST_DECLARE_VAR) {
        ERROR(funcdecl, "Internal error expected declaration");
        return orig_funcdecl;
    }
    if (funcdecl->left->kind != AST_FUNCTYPE) {
        ERROR(funcdecl, "Expected function declaration");
        return orig_funcdecl;
    }
    funcdecl = funcdecl->left;
    param_list = funcdecl->right;
    while(param_list) {
        AST *newdecl;
        param = param_list->left;
        if (!param || param->kind != AST_DECLARE_VAR) {
            ERROR(param, "Internal error, expecting to find variable");
            return orig_funcdecl;
        }
        // find the corresponding one from decl_list and replace it
        newdecl = FindDeclInList(param->right, decl_list);
        if (newdecl) {
            param_list->left = newdecl;
        }
        param_list = param_list->right;
    }
    return orig_funcdecl;
}

static int IsStatic(AST *ftype) {
    return ftype && ftype->kind == AST_STATIC;
}

/* declare a typed function, optionally using a local identifier (if the function is STATIC) */
static AST *
DeclareCTypedFunction(Module *P, AST *ftype, AST *nameAst, int is_public, AST *body, AST *attribute)
{
    if (IsStatic(ftype) && nameAst->kind == AST_IDENTIFIER) {
        /* declare a local alias for the name */
        const char *nameString = GetIdentifierName(nameAst);
        AST *globalName = AstTempIdentifier(nameString);
        AST *origName = nameAst;
        EnterLocalAlias(currentTypes, globalName, nameString);
        nameAst = NewAST(AST_LOCAL_IDENTIFIER, globalName, nameAst);
        // we have to fix up the body of the function so that references
        // to the original name are replaced with the new name
        ReplaceAst(body, origName, nameAst);
    }
    return DeclareTypedFunction(P, ftype, nameAst, is_public, body, attribute, NULL);
}

static AST *
ConstructDefaultValue(AST *decl, AST *val)
{
    AST *ident;
    AST *ast;
    if (!val) {
        return decl;
    }
    if (!decl || decl->kind != AST_DECLARE_VAR) {
        ERROR(decl, "Internal error: expected variable declaration");
        return decl;
    }
    ident = decl->right;
    ast = NewAST(AST_ASSIGN, ident, val);
    decl->right = ast;
    return decl;
}

%}

%token C_IDENTIFIER "identifier"
%token C_CONSTANT   "constant"
%token C_STRING_LITERAL "string literal"
%token C_SIZEOF     "sizeof"

%token C_PTR_OP "->"
%token C_INC_OP "++"
%token C_DEC_OP "--"
%token C_LEFT_OP "<<"
%token C_RIGHT_OP ">>"
%token C_LE_OP "<="
%token C_GE_OP ">="
%token C_EQ_OP "=="
%token C_NE_OP "!="
%token C_AND_OP "&&"
%token C_OR_OP "||"
%token C_MUL_ASSIGN "*="
%token C_DIV_ASSIGN "/="
%token C_MOD_ASSIGN "%="
%token C_ADD_ASSIGN "+="
%token C_SUB_ASSIGN "-="
%token C_LEFT_ASSIGN "<<="
%token C_RIGHT_ASSIGN ">>="
%token C_AND_ASSIGN "&="
%token C_XOR_ASSIGN "^="
%token C_OR_ASSIGN "|="
%token C_TYPE_NAME "type name"

%token C_LIMITMIN_OP "#<"
%token C_LIMITMAX_OP "#>"

%token C_TYPEDEF "typedef"
%token C_EXTERN "extern"
%token C_STATIC "static"
%token C_AUTO "auto"
%token C_REGISTER "register"
%token C_RESTRICT "__restrict"
%token C_BOOL "_Bool"
%token C_CHAR  "char"
%token C_SHORT "short"
%token C_IMAGINARY "_Imaginary"
%token C_INLINE "inline"
%token C_INT   "int"
%token C_LONG  "long"
%token C_SIGNED "signed"
%token C_UNSIGNED "unsigned"
%token C_FLOAT  "float"
%token C_DOUBLE "double"
%token C_CONST "const"
%token C_VOLATILE "volatile"
%token C_VOID "void"
%token C_STRUCT "struct"
%token C_UNION "union"
%token C_ENUM "enum"
%token C_ELLIPSIS "..."

%token C_CASE "case"
%token C_DEFAULT "default"
%token C_IF "if"
%token C_ELSE "else"
%token C_SWITCH "switch"
%token C_WHILE "while"
%token C_DO    "do"
%token C_FOR   "for"
%token C_GOTO  "goto"
%token C_CONTINUE "continue"
%token C_BREAK "break"
%token C_RETURN "return"

%token C_FROMFILE "__fromfile"
%token C_USING "__using"
%token C_ATTRIBUTE "__attribute__"

%token C_ASM "__asm"
%token C_PASM "__pasm"
%token C_INSTR "asm instruction"
%token C_INSTRMODIFIER "instruction modifier"
%token C_HWREG "hardware register"

// C++ tokens
%token C_CATCH "catch"
%token C_CLASS "class"
%token C_DELETE "delete"
%token C_NEW "new"
%token C_NULLPTR "nullptr"
%token C_PRIVATE "private"
%token C_PUBLIC "public"
%token C_TEMPLATE "template"
%token C_THIS "this"
%token C_THROW "throw"

// asm only tokens
%token C_ALIGNL "alignl"
%token C_ALIGNW "alignw"
%token C_BYTE "byte"
%token C_FILE "file"
%token C_FIT "fit"
%token C_ORG  "org"
%token C_ORGH "orgh"
%token C_ORGF "orgf"
%token C_RES "res"
%token C_WORD "word"
%token C_EOLN "end of line"

// builtin functions
%token C_BUILTIN_ABS    "__builtin_abs"
%token C_BUILTIN_CLZ    "__builtin_clz"
%token C_BUILTIN_SQRT   "__builtin_sqrt"

%token C_BUILTIN_ALLOCA "__builtin_alloca"
%token C_BUILTIN_COGSTART "__builtin_cogstart"
%token C_BUILTIN_EXPECT "__builtin_expect"
%token C_BUILTIN_PRINTF "__builtin_printf"
%token C_BUILTIN_REV    "__builtin_rev"
%token C_BUILTIN_VA_START "__builtin_va_start"
%token C_BUILTIN_VA_ARG   "__builtin_va_arg"
%token C_BUILTIN_SETJMP   "__builtin_setjmp"
%token C_BUILTIN_LONGJMP  "__builtin_longjmp"

%token C_EOF "end of file"

%start translation_unit

%%

primary_expression
	: C_IDENTIFIER
            { $$ = $1; }
	| C_CONSTANT
            { $$ = $1; }
	| C_HWREG
            { $$ = $1; }
	| C_THIS
            { $$ = NewAST(AST_SELF, NULL, NULL); }
        | C_NULLPTR
            { $$ = AstBitValue(0); }
	| C_STRING_LITERAL
            { $$ = NewAST(AST_STRINGPTR, $1, NULL); }
        | C_BUILTIN_PRINTF
            { $$ = NewAST(AST_PRINT, NULL, NULL); }
	| '(' expression ')'
            { $$ = $2; }
	| '(' '{' block_item_list '}' ')'
            { $$ = $3; }
	;

postfix_expression
	: primary_expression
            { $$ = $1; }
        | C_BUILTIN_ABS '(' assignment_expression ')'
            { $$ = AstOperator(K_ABS, NULL, $3); }
        | C_BUILTIN_CLZ '(' assignment_expression ')'
            {
                AST *expr = AstOperator(K_ENCODE, NULL, $3);
                expr = AstOperator('-', AstInteger(32), expr);
                $$ = expr;
            }
        | C_BUILTIN_SQRT '(' assignment_expression ')'
            { $$ = AstOperator(K_SQRT, NULL, $3); }
        | C_BUILTIN_REV '(' argument_expression_list ')'
            {
                AST *list;
                AST *arg1, *arg2;
                
                list = $3;
                if (!list || !list->left)
                {
                    SYNTAX_ERROR("Missing argument to __builtin_rev");
                    arg1 = AstInteger(1);
                } else {
                    arg1 = list->left;
                }
                if (list && list->right) {
                    arg2 = list->right->left;
                    if (list->right->right) {
                        SYNTAX_ERROR("Too many arguments to __builtin_rev");
                    }
                } else {
                    arg2 = AstInteger(0);
                }

                // PropGCC defines __builtin_rev to match the ASM instruction,
                // not the >< operator, hence the 32 - arg2
                $$ = AstOperator(K_REV, arg1, AstOperator('-', AstInteger(32), arg2));
            }
        | C_BUILTIN_VA_START '(' unary_expression ',' C_IDENTIFIER ')'
            {
                $$ = NewAST(AST_VA_START, $3, $5);
            }
        | C_BUILTIN_VA_ARG '(' assignment_expression ',' type_name ')'
            {
                // NOTE: like an AST_MEMREF, the type goes first
                $$ = NewAST(AST_VA_ARG, $5, $3);
            }
        | C_BUILTIN_SETJMP '(' assignment_expression ')'
            {
                $$ = NewAST(AST_SETJMP, $3, NULL);
            }
        | C_BUILTIN_ALLOCA '(' assignment_expression ')'
            { $$ = NewAST(AST_ALLOCA, ast_type_ptr_void, $3); }
        | C_BUILTIN_COGSTART '(' argument_expression_list ')'
            {
                AST *elist;
                AST *immval = AstInteger(0x1e);
                elist = NewAST(AST_EXPRLIST, immval, NULL);
                elist = AddToList(elist, $3);
                $$ = NewAST(AST_COGINIT, elist, NULL);
            }
        | C_BUILTIN_EXPECT '(' assignment_expression ',' assignment_expression ')'
            { $$ = $3; }
        | C_BUILTIN_LONGJMP '(' assignment_expression ',' assignment_expression ')'
            {
                $$ = NewAST(AST_THROW, $5, $3);
            }
	| postfix_expression '[' expression ']'
            { $$ = NewAST(AST_ARRAYREF, $1, $3); }
	| postfix_expression '(' ')'
            { $$ = NewAST(AST_FUNCCALL, $1, NULL); }
	| postfix_expression '(' argument_expression_list ')'
            { $$ = NewAST(AST_FUNCCALL, $1, $3); }
	| postfix_expression '.' any_identifier
            { $$ = NewAST(AST_METHODREF, $1, $3); }
	| postfix_expression C_PTR_OP any_identifier
            { $$ = NewAST(AST_METHODREF,
                          NewAST(AST_ARRAYREF, $1, AstInteger(0)),
                          $3);
            }
	| postfix_expression C_INC_OP
            { $$ = AstOperator(K_INCREMENT, $1, NULL); }
	| postfix_expression C_DEC_OP
            { $$ = AstOperator(K_DECREMENT, $1, NULL); }
        | '(' type_name ')' '{' initializer_list '}'
            {
                /* FIXME: treat this like ({ type_name t = initializer_list; t; }) */
                AST *typ = $2;
                AST *initlist = $5;
                AST *id = AstTempIdentifier("_initval_");

                AST *decl;
                AST *stmt;

                decl = SingleDeclareVar(typ, id);
                stmt = NewAST(AST_STMTLIST, decl, NULL);
                stmt = AddToList(stmt,
                                 NewAST(AST_STMTLIST, AstAssign(id, initlist), NULL));
                stmt = AddToList(stmt, NewAST(AST_STMTLIST, id, NULL));
                $$ = stmt;
            }
        | '(' type_name ')' '{' initializer_list ',' '}'
            {
                /* treat this like ({ type_name t = initializer_list; t; }) */
                AST *typ = $2;
                AST *initlist = $5;
                AST *id = AstTempIdentifier("_initval_");

                AST *decl = AstAssign(id, initlist);
                AST *stmt;

                DeclareOneGlobalVar(current, decl, typ, IN_DAT);
                stmt = NewAST(AST_STMTLIST, id, NULL);
                $$ = stmt;
            }
	;

argument_expression_list
	: assignment_expression
            { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
	| argument_expression_list ',' assignment_expression
            { $$ = AddToList($1, NewAST(AST_EXPRLIST, $3, NULL)); }
	;

unary_expression
	: postfix_expression
            { $$ = $1; }
	| C_INC_OP unary_expression
            { $$ = AstOperator(K_INCREMENT, NULL, $2); }
	| C_DEC_OP unary_expression
            { $$ = AstOperator(K_DECREMENT, NULL, $2); }
	| unary_operator cast_expression
            {
                $$ = $1;
                if ($$->kind == AST_ADDROF || $$->kind == AST_ARRAYREF)
                    $$->left = $2;
                else
                    $$->right = $2;
            }
	| C_SIZEOF '(' type_name ')'
            { $$ = NewAST(AST_SIZEOF, $3, NULL); }           
	| C_SIZEOF unary_expression
            { $$ = NewAST(AST_SIZEOF, $2, NULL); }           
	;

unary_operator
	: '&'
            { $$ = NewAST(AST_ADDROF, NULL, NULL); }
	| '@'
            { $$ = NewAST(AST_ADDROF, NULL, NULL); }
	| '*'
            { $$ = NewAST(AST_ARRAYREF, NULL, AstInteger(0)); }
	| '+'
            { $$ = AstOperator('+', NULL, NULL); }
	| '-'
            { $$ = AstOperator(K_NEGATE, NULL, NULL); }
	| '~'
            { $$ = AstOperator(K_BIT_NOT, NULL, NULL); }
	| '!'
            { $$ = AstOperator(K_BOOL_NOT, NULL, NULL); }
	;

cast_expression
	: unary_expression
            { $$ = $1; }
	| '(' type_name ')' cast_expression
            { $$ = NewAST(AST_CAST, $2, $4); }
	;

multiplicative_expression
	: cast_expression
            { $$ = $1; }
	| multiplicative_expression '*' cast_expression
            { $$ = AstOperator('*', $1, $3); }
	| multiplicative_expression '/' cast_expression
            { $$ = AstOperator('/', $1, $3); }
	| multiplicative_expression '%' cast_expression
            { $$ = AstOperator(K_MODULUS, $1, $3); }
	;

additive_expression
	: multiplicative_expression
            { $$ = $1; }
	| additive_expression '+' multiplicative_expression
            { $$ = AstOperator('+', $1, $3); }
	| additive_expression '-' multiplicative_expression
            { $$ = AstOperator('-', $1, $3); }
	;

shift_expression
	: additive_expression
            { $$ = $1; }
	| shift_expression C_LEFT_OP additive_expression
            { $$ = AstOperator(K_SHL, $1, $3); }
	| shift_expression C_RIGHT_OP additive_expression
            { $$ = AstOperator(K_SAR, $1, $3); }
	;

relational_expression
	: shift_expression
            { $$ = $1; }
	| relational_expression '<' shift_expression
            { $$ = AstOperator('<', $1, $3); }
	| relational_expression '>' shift_expression
            { $$ = AstOperator('>', $1, $3); }
	| relational_expression C_LE_OP shift_expression
            { $$ = AstOperator(K_LE, $1, $3); }
	| relational_expression C_GE_OP shift_expression
            { $$ = AstOperator(K_GE, $1, $3); }
	;

equality_expression
	: relational_expression
            { $$ = $1; }
	| equality_expression C_EQ_OP relational_expression
            { $$ = AstOperator(K_EQ, $1, $3); }
	| equality_expression C_NE_OP relational_expression
            { $$ = AstOperator(K_NE, $1, $3); }
	;

and_expression
	: equality_expression
            { $$ = $1; }
	| and_expression '&' equality_expression
            { $$ = AstOperator('&', $1, $3); }
	;

exclusive_or_expression
	: and_expression
            { $$ = $1; }
	| exclusive_or_expression '^' and_expression
            { $$ = AstOperator('^', $1, $3); }
	;

inclusive_or_expression
	: exclusive_or_expression
            { $$ = $1; }
	| inclusive_or_expression '|' exclusive_or_expression
            { $$ = AstOperator('|', $1, $3); }
	;

logical_and_expression
	: inclusive_or_expression
            { $$ = $1; }
	| logical_and_expression C_AND_OP inclusive_or_expression
            { $$ = AstOperator(K_BOOL_AND, $1, $3); }
	;

logical_or_expression
	: logical_and_expression
            { $$ = $1; }
	| logical_or_expression C_OR_OP logical_and_expression
            { $$ = AstOperator(K_BOOL_OR, $1, $3); }
	;

conditional_expression
	: logical_or_expression
            { $$ = $1; }
	| logical_or_expression '?' expression ':' conditional_expression
            { $$ = NewAST(AST_CONDRESULT, $1, NewAST(AST_THENELSE, $3, $5)); }
	;

assignment_expression
	: conditional_expression
            { $$ = $1; }
	| unary_expression assignment_operator assignment_expression
            { $$ = $2; $$->left = $1; $$->right = $3; }        
	;

assignment_operator
	: '='
            { $$ = AstAssign(NULL, NULL); }
	| C_MUL_ASSIGN
            { $$ = AstOpAssign('*', NULL, NULL); }
	| C_DIV_ASSIGN
            { $$ = AstOpAssign('/', NULL, NULL); }
	| C_MOD_ASSIGN
            { $$ = AstOpAssign(K_MODULUS, NULL, NULL); }
	| C_ADD_ASSIGN
            { $$ = AstOpAssign('+', NULL, NULL); }
	| C_SUB_ASSIGN
            { $$ = AstOpAssign('-', NULL, NULL); }
	| C_LEFT_ASSIGN
            { $$ = AstOpAssign(K_SHL, NULL, NULL); }
	| C_RIGHT_ASSIGN
            { $$ = AstOpAssign(K_SAR, NULL, NULL); }
	| C_AND_ASSIGN
            { $$ = AstOpAssign('&', NULL, NULL); }
	| C_XOR_ASSIGN
            { $$ = AstOpAssign('^', NULL, NULL); }
	| C_OR_ASSIGN
            { $$ = AstOpAssign('|', NULL, NULL); }
	;

expression
	: assignment_expression
            { $$ = $1; }
	| expression ',' assignment_expression
            { $$ = NewAST(AST_SEQUENCE, $1, $3); }
	;

constant_expression
	: conditional_expression
            { $$ = $1; }
	;

declaration
	: declaration_specifiers ';'
            { $$ = SingleDeclareVar(NULL, $1); }
	| declaration_specifiers init_declarator_list ';'
            {
                $$ = MultipleDeclareVar($1, $2);
            }
	;

declaration_specifiers
	: storage_class_specifier
            { $$ = $1; }
	| storage_class_specifier declaration_specifiers
            {
                AST *prefix = $1;
                AST *suffix = $2;
                $$ = MergePrefix(prefix, suffix);
            }
	| type_specifier reset_identifier_expectation
            { $$ = $1; AllowTypeNames(); }
	| type_specifier reset_identifier_expectation declaration_specifiers
            { $$ = C_ModifySignedUnsigned($1, $3); AllowTypeNames(); }
	| type_qualifier
            { $$ = $1; $$->left = ast_type_long; }
	| type_qualifier declaration_specifiers
            { $$ = MergePrefix($1, $2); }
	;

reset_identifier_expectation
        :
            { DisallowTypeNames(); }
;

init_declarator_list
	: init_declarator
            { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
	| init_declarator_list ',' init_declarator
            { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $3, NULL)); }
	;

init_declarator
	: declarator
            { $$ = $1; }
	| declarator '=' initializer
            { $$ = AstAssign($1, $3); }        
	;

storage_class_specifier
	: C_TYPEDEF
            { $$ = NewAST(AST_TYPEDEF, NULL, NULL); }
	| C_EXTERN
            { $$ = NewAST(AST_EXTERN, NULL, NULL); }
	| C_STATIC
            { $$ = NewAST(AST_STATIC, NULL, NULL); }
	| C_AUTO
            { $$ = NULL; }
	| C_INLINE
            { $$ = NULL; }
	| C_REGISTER
            { $$ = NULL; }
	;

type_specifier
	: C_VOID
            { $$ = ast_type_void; }
	| C_CHAR
            { $$ = ast_type_byte; }
	| C_BOOL
            { $$ = ast_type_byte; }
	| C_SHORT
            { $$ = ast_type_signed_word; }
	| C_INT
            { $$ = ast_type_long; }
	| C_LONG
            {
                if (!ast_type_c_long) {
                    // same as ast_type_long, but a distinct memory address
                    // so we can distinguish it
                    ast_type_c_long = NewAST(AST_INTTYPE, AstInteger(4), NULL);
                }
                $$ = ast_type_c_long;
            }
	| C_FLOAT
            { $$ = ast_type_float; }
	| C_DOUBLE
            { $$ = ast_type_float; }
	| C_SIGNED
            {
                if (!ast_type_c_signed) {
                    // same as ast_type_long, but a distinct memory address
                    // so we can distinguish it
                    ast_type_c_signed = NewAST(AST_INTTYPE, AstInteger(4), NULL);
                }
                $$ = ast_type_c_signed;
            }
	| C_UNSIGNED
            { $$ = ast_type_unsigned_long; }
	| struct_or_union_specifier
            { $$ = $1; }
	| enum_specifier
            { $$ = $1; }
	| C_TYPE_NAME
            {
                AST *ident = $1;
                Symbol *sym = LookupSymbolInTable(currentTypes, ident->d.string);
                if (sym && sym->kind == SYM_TYPEDEF) {
                    $$ = (AST *)sym->val;
                } else {
                    SYNTAX_ERROR("Internal error, bad typename %s", ident->d.string);
                    $$ = NULL;
                }
            }
	;

struct_or_union_specifier
	: struct_open struct_declaration_list '}' struct_close
            {
                AST *d = $1;
                AddStructBody(current, $2);
                current = current->superclass;
                $$ = d;
            }
	| struct_or_union any_identifier
            { $$ = MakeNewStruct(current, $1, $2, NULL); }
        | struct_or_union C_USING fromfile_clause
            { $$ = MakeNewStruct(current, $1, NULL, $3); }
	;

any_identifier
        : C_IDENTIFIER
            { $$ = $1; }
        | C_TYPE_NAME
            { $$ = $1; }
;
struct_or_union
	: C_STRUCT
            {
                AST *c = NewAST(AST_STRUCT, NULL, NULL);
                c->d.ival = 0;  // public by default
                $$ = c;
            }
	| C_CLASS
            {
                AST *c = NewAST(AST_STRUCT, NULL, NULL);
                c->d.ival = 1;  // private by default
                $$ = c;
            }
	| C_UNION
            { $$ = NewAST(AST_UNION, NULL, NULL); }
	;

fromfile_clause
        : '(' C_STRING_LITERAL ')'
            {
                AST *str = $2;
                if (str && str->kind == AST_EXPRLIST) {
                    str = str->left;
                }
                $$ = str;
            }
        ;

struct_open
        : struct_or_union any_identifier '{'
            {
                AST *newstruct;
                Module *C;
                newstruct = MakeNewStruct(current, $1, $2, NULL);
                $$ = newstruct;
                C = GetClassPtr(newstruct);
                C->superclass = current;
                current = C;
                PushCurrentTypes();
            }
        | struct_or_union '{'
            {
                AST *newstruct;
                Module *C;
                newstruct = MakeNewStruct(current, $1, NULL, NULL);
                $$ = newstruct;
                C = GetClassPtr(newstruct);
                C->superclass = current;
                current = C;
                PushCurrentTypes();
            }
;

struct_close:
    {
        PopCurrentTypes();
    }
;

struct_declaration_list
	: struct_declaration
           { $$ = $1; }
	| struct_declaration_list struct_declaration
           { $$ = AddToList($1, $2); }
	;

struct_declaration
        : specifier_qualifier_list ';' /* for anonymous struct/union */
            {
                AST *dummy;
                dummy = AstTempIdentifier("__anonymous__");
                dummy = NewAST(AST_LISTHOLDER, dummy, NULL);
                $$ = MultipleDeclareVar($1, dummy);
            }
	| specifier_qualifier_list struct_declarator_list ';'
            {
                $$ = MultipleDeclareVar($1, $2);
            }
        | C_PUBLIC ':'
            {
                $$ = NewAST(AST_STMTLIST,
                            NewAST(AST_PUBFUNC, NULL, NULL), NULL);
            }
        | C_PRIVATE ':'
            {
                $$ = NewAST(AST_STMTLIST,
                            NewAST(AST_PRIFUNC, NULL, NULL), NULL);
            }
        | specifier_qualifier_list struct_declarator_list compound_statement
            {
                AST *type;
                AST *ident;
                AST *body = $3;
                AST *decl = $2;
                AST *spqual = $1;
                AST *top_decl;

                if (decl->right) {
                    SYNTAX_ERROR("bad method declaration");
                }
                type = CombineTypes(spqual, decl->left, &ident);

                top_decl = NewAST(AST_LISTHOLDER, type,
                                  NewAST(AST_LISTHOLDER, ident,
                                         NewAST(AST_LISTHOLDER, body, NULL)));
                top_decl = NewAST(AST_FUNCDECL, top_decl, NULL);
                $$ = NewAST(AST_STMTLIST, top_decl, NULL);
            }        
	;

specifier_qualifier_list
	: type_specifier specifier_qualifier_list
            { $$ = C_ModifySignedUnsigned($1, $2); }
	| type_specifier
            { $$ = $1; }
	| type_qualifier specifier_qualifier_list
            { $$ = $1; $$->left = $2; }
	| type_qualifier
            { $$ = $1; $$->left = ast_type_long; }
	;

struct_declarator_list
	: struct_declarator
            { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
	| struct_declarator_list ',' struct_declarator
            { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $3, NULL)); }
	;

struct_declarator
	: declarator
            { $$ = $1; }
	| ':' constant_expression
            { SYNTAX_ERROR("Empty bitfields not supported yet"); $$ = NULL; }
	| declarator ':' constant_expression
            {
                $$ = NewAST(AST_BITFIELD, $1, $3);
            }
	;

enum_specifier
	: C_ENUM '{' enumerator_list '}'
            { $$ = AddEnumerators(NULL, $3); }
	| C_ENUM any_identifier '{' enumerator_list '}'
            { $$ = AddEnumerators($2, $4); }
	| C_ENUM '{' enumerator_list ',' '}'
            { $$ = AddEnumerators(NULL, $3); }
	| C_ENUM any_identifier '{' enumerator_list ',' '}'
            { $$ = AddEnumerators($2, $4); }
	| C_ENUM any_identifier
            { $$ = ast_type_long; }
	;

enumerator_list
	: enumerator
            { $$ = $1; }
	| enumerator_list ',' enumerator
            { $$ = AddToList($1, $3); }
	;

enumerator
	: C_IDENTIFIER
            { $$ = CommentedListHolder($1); }
	| C_IDENTIFIER '=' constant_expression
            {
                AST *setval = NewAST(AST_ENUMSET, $3, NULL);
                AST *id = CommentedListHolder($1);
                setval = NewAST(AST_LISTHOLDER, setval, id);
                $$ = setval;
            }
	;

type_qualifier
	: C_CONST
            { $$ = NewAST(AST_MODIFIER_CONST, NULL, NULL); }
	| C_VOLATILE
            { $$ = NewAST(AST_MODIFIER_VOLATILE, NULL, NULL); }
        | C_RESTRICT
            { $$ = NULL; }
	;

declarator
	: pointer direct_declarator
            {  $$ = CombinePointer($1, $2); }
	| direct_declarator
            { $$ = $1; }
	;

direct_declarator
	: C_IDENTIFIER
            { $$ = $1; }
	| '(' declarator ')'
            { $$ = $2; }
	| direct_declarator '[' constant_expression ']'
            { $$ = NewAST(AST_ARRAYDECL, $1, $3); }
	| direct_declarator '[' ']'
            { $$ = NewAST(AST_ARRAYDECL, $1, NULL); }
	| direct_declarator '(' parameter_type_list ')'
            { $$ = NewAST(AST_DECLARE_VAR, NewAST(AST_FUNCTYPE, NULL, ProcessParamList($3)), $1); }
	| direct_declarator '(' identifier_list ')'
            { $$ = NewAST(AST_DECLARE_VAR, NewAST(AST_FUNCTYPE, NULL, ProcessParamList($3)), $1); }
	| direct_declarator '(' ')'
            { $$ = NewAST(AST_DECLARE_VAR, NewAST(AST_FUNCTYPE, NULL, NULL), $1); }
	;

pointer
	: '*'
            { $$ = NewAST(AST_PTRTYPE, NULL, NULL); }
	| '*' type_qualifier_list
            {
                $$ = CombinePointer(NewAST(AST_PTRTYPE, NULL, NULL), $2);
            }
	| '*' pointer
            { $$ = NewAST(AST_PTRTYPE, $2, NULL); }
	| '*' type_qualifier_list pointer
            {
                AST *q = $2;
                while (q && q->left)
                    q = q->left;
                if (q) q->left = $3;
                $$ = NewAST(AST_PTRTYPE, $2, NULL);
            }
	| '&'
            { $$ = NewAST(AST_REFTYPE, NULL, NULL); }
	| '&' type_qualifier_list
            {
                $$ = CombinePointer(NewAST(AST_REFTYPE, NULL, NULL), $2);
            }
	| '&' pointer
            { $$ = NewAST(AST_REFTYPE, $2, NULL); }
	| '&' type_qualifier_list pointer
            {
                AST *q = $2;
                while (q && q->left)
                    q = q->left;
                if (q) q->left = $3;
                $$ = NewAST(AST_REFTYPE, $2, NULL);
            }
	;

type_qualifier_list
	: type_qualifier
           { $$ = $1; }
	| type_qualifier_list type_qualifier
           { $$ = $1; $$->left = $2; }
	;


parameter_type_list
	: parameter_list
           { $$ = $1; }
	| parameter_list ',' C_ELLIPSIS
            { $$ = AddToList($1,
                             NewAST(AST_LISTHOLDER,
                                    NewAST(AST_VARARGS, NULL, NULL),
                                    NULL));
            }
	;

parameter_list
	: parameter_declaration
            { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
	| parameter_list ',' parameter_declaration
            { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $3, NULL)); }
	;

raw_parameter_declaration
	: declaration_specifiers declarator
            { $$ = SingleDeclareVar($1, $2); }
	| declaration_specifiers abstract_declarator
            { $$ = SingleDeclareVar($1, $2); }
	| declaration_specifiers
            { $$ = $1; }
;
parameter_declaration
        : raw_parameter_declaration opt_param_default
            {
                AST *assign = $2;
                AST *decl = $1;
                $$ = ConstructDefaultValue(decl, assign);
            }
;
opt_param_default
        : '=' conditional_expression
            { $$ = $2; }
        | /* empty */
            { $$ = NULL; }
;

identifier_list
	: C_IDENTIFIER
            { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
	| identifier_list ',' C_IDENTIFIER
            { $$ = AddToList($1, NewAST(AST_EXPRLIST, $3, NULL)); }
	;

type_name
	: specifier_qualifier_list
            { $$ = $1; }
	| specifier_qualifier_list abstract_declarator
            { $$ = CombineTypes($1, $2, NULL); }
	;

abstract_declarator
	: pointer
            { $$ = $1; }
	| direct_abstract_declarator
            { $$ = $1; }
	| pointer direct_abstract_declarator
            { $$ = CombinePointer($1, $2); }
	;

direct_abstract_declarator
	: '(' abstract_declarator ')'
	| '[' ']'
	| '[' constant_expression ']'
	| direct_abstract_declarator '[' ']'
	| direct_abstract_declarator '[' constant_expression ']'
	| '(' ')'
	| '(' parameter_type_list ')'
	| direct_abstract_declarator '(' ')'
	| direct_abstract_declarator '(' parameter_type_list ')'
	;

initializer
	: assignment_expression
            { $$ = $1; }
	| '{' initializer_list '}'
            { $$ = $2; }
	| '{' initializer_list ',' '}'
            { $$ = $2; }
	;

initializer_list
	: initializer
            {
                AST *ast = NewAST(AST_EXPRLIST, $1, NULL);
                $$ = ast;
            }
        | designation initializer
            {
                AST *desig = $1;
                AST *initval = $2;
                AST *ast;

                ast = NewAST(AST_INITMODIFIER, desig, initval);
                ast = NewAST(AST_EXPRLIST, ast, NULL);
                $$ = ast;
            }
	| initializer_list ',' initializer
            {
                AST *list = $1;
                AST *ast = $3;
                ast = NewAST(AST_EXPRLIST, ast, NULL);
                $$ = AddToListEx(list, ast, (AST **)&list->d.ptr);
            }
        | initializer_list ',' designation initializer
            {
                AST *list = $1;
                AST *desig = $3;
                AST *initval = $4;
                AST *ast;
                
                ast = NewAST(AST_INITMODIFIER, desig, initval);
                ast = NewAST(AST_EXPRLIST, ast, NULL);
                $$ = AddToListEx(list, ast, (AST **)&list->d.ptr);
            }
	;

designation
        : designator_list '='
            { $$ = $1; };
        ;

/* a designator list like [2].x should come out as
 * ARRAYREF( METHODREF (NULL, x), 2 )
 */

designator_list
        : designator
            { $$ = $1; }
        | designator_list designator
            {
                AST *upper = $1;
                AST *lower = $2;
                upper->left = lower;
                $$ = upper;
            }
        ;

designator
         : '[' constant_expression ']'
            { $$ = NewAST(AST_ARRAYREF, NULL, $2); }
         | '.' any_identifier
            { $$ = NewAST(AST_METHODREF, NULL, $2); }
         ;

statement
	: labeled_statement
	| compound_statement
	| expression_statement
	| selection_statement
	| iteration_statement
	| jump_statement
        | asm_statement
	;

labeled_statement
	: any_identifier ':' statement
            {
                AST *label = NewAST(AST_LABEL, LabelName($1), NULL);
                $$ = NewAST(AST_STMTLIST, label,
                              NewAST(AST_STMTLIST, $3, NULL));
            }
	| C_CASE constant_expression ':' statement
            {
                $$ = NewAST(AST_CASEITEM, $2, $4);
            }
	| C_DEFAULT ':' statement
            {
                $$ = NewAST(AST_OTHER, $3, NULL);
            }
	;

compound_statement
	: compound_statement_open compound_statement_close
            { $$ = NULL; }
	| compound_statement_open block_item_list compound_statement_close
            { $$ = $2; }
	;

compound_statement_open:
  '{'
    { PushCurrentTypes(); }
  ;

compound_statement_close:
  '}'
    { PopCurrentTypes(); }
  ;

for_statement_start:
  C_FOR
  { PushCurrentTypes(); }
;

block_item_list
   : block_item
       { $$ = $1; }
   | block_item_list block_item
       { $$ = AddToList($1, $2); }
   ;

block_item
   : declaration
       {
           AST *decl = MakeDeclarations($1, currentTypes);
           $$ = decl;
       }
   | statement
       { $$ = NewAST(AST_STMTLIST, $1, NULL); }
   ;

asm_statement:
  C_ASM opt_asm_volatile '{' asmlist '}'
    {
        AST *asmcode;
        AST *vol = $2;
        asmcode = NewAST(AST_INLINEASM, $4, vol);
        $$ = asmcode;
    }
  | C_ASM opt_asm_volatile C_EOLN '{' asmlist '}'
    {
        AST *asmcode;
        AST *vol = $2;
        asmcode = NewAST(AST_INLINEASM, $5, vol);
        $$ = asmcode;
    }
  ;

opt_asm_volatile:
  /* nothing */
    { $$ = 0; }
  | C_VOLATILE
    { $$ = AstInteger(3); }
  | C_CONST
    { $$ = AstInteger(1); }
;

top_asm:
  C_ASM '{' asmlist '}'
      { $$ = current->datblock = AddToListEx(current->datblock, $3, &current->datblock_tail); }
  | C_ASM C_EOLN '{' asmlist '}'
      { $$ = current->datblock = AddToListEx(current->datblock, $4, &current->datblock_tail); }
;

asmlist:
  asmline
  { $$ = $1; }
  | asmlist asmline
  { $$ = AddToList($1, $2); }
  ;

asmline:
  asm_baseline
  | C_IDENTIFIER asm_baseline
    {   AST *linebreak;
        AST *comment = GetComments();
        AST *ast;
        ast = $1;
        if (comment && (comment->d.string || comment->kind == AST_SRCCOMMENT)) {
            linebreak = NewCommentedAST(AST_LINEBREAK, NULL, NULL, comment);
        } else {
            linebreak = NewAST(AST_LINEBREAK, NULL, NULL);
        }
        ast = AddToList(ast, $2);
        ast = AddToList(linebreak, ast);
        $$ = ast;
    }
  ;

asm_baseline:
  C_EOLN
    { $$ = NULL; }
  | error C_EOLN
    { $$ = NULL; }
  | C_BYTE C_EOLN
    { $$ = NewCommentedAST(AST_BYTELIST, NULL, NULL, $1); }
  | C_BYTE asm_operandlist C_EOLN
    { $$ = NewCommentedAST(AST_BYTELIST, $2, NULL, $1); }
  | C_WORD C_EOLN
    { $$ = NewCommentedAST(AST_WORDLIST, NULL, NULL, $1); }
  | C_WORD asm_operandlist C_EOLN
    { $$ = NewCommentedAST(AST_WORDLIST, $2, NULL, $1); }
  | C_LONG C_EOLN
    { $$ = NewCommentedAST(AST_LONGLIST, NULL, NULL, $1); }
  | C_LONG asm_operandlist C_EOLN
    { $$ = NewCommentedAST(AST_LONGLIST, $2, NULL, $1); }
  | instruction C_EOLN
    { $$ = NewCommentedInstr($1); }
  | instruction asm_operandlist C_EOLN
    { $$ = NewCommentedInstr(AddToList($1, $2)); }
  | instruction modifierlist C_EOLN
    { $$ = NewCommentedInstr(AddToList($1, $2)); }
  | instruction asm_operandlist modifierlist C_EOLN
    { $$ = NewCommentedInstr(AddToList($1, AddToList($2, $3))); }
  | C_ALIGNL C_EOLN
    { $$ = NewCommentedAST(AST_ALIGN, AstInteger(4), NULL, $1); }
  | C_ALIGNW C_EOLN
    { $$ = NewCommentedAST(AST_ALIGN, AstInteger(2), NULL, $1); }
  | C_ORG C_EOLN
    { $$ = NewCommentedAST(AST_ORG, NULL, NULL, $1); }
  | C_ORG asmexpr C_EOLN
    { $$ = NewCommentedAST(AST_ORG, $2, NULL, $1); }
  | C_ORGH C_EOLN
    { $$ = NewCommentedAST(AST_ORGH, NULL, NULL, $1); }
  | C_ORGH asmexpr C_EOLN
    { $$ = NewCommentedAST(AST_ORGH, $2, NULL, $1); }
  | C_ORGF asmexpr C_EOLN
    { $$ = NewCommentedAST(AST_ORGF, $2, NULL, $1); }
  | C_RES asmexpr C_EOLN
    { $$ = NewCommentedAST(AST_RES, $2, NULL, $1); }
  | C_FIT asmexpr C_EOLN
    { $$ = NewCommentedAST(AST_FIT, $2, NULL, $1); }
  | C_FIT C_EOLN
    { $$ = NewCommentedAST(AST_FIT, AstInteger(0x1f0), NULL, $1); }
  | C_FILE C_STRING_LITERAL C_EOLN
    { $$ = NewCommentedAST(AST_FILE, GetFullFileName($2), NULL, $1); }
  ;

asm_operand:
  asmexpr
   { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
 | '#' asmexpr
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_IMMHOLDER, $2, NULL), NULL); }
 | '#' '#' asmexpr
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_BIGIMMHOLDER, $3, NULL), NULL); }
 | asmexpr '[' asmexpr ']'
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_ARRAYREF, $1, $3), NULL); }
;

asm_operandlist:
   asm_operand
   { $$ = $1; }
 | asm_operandlist ',' asm_operand
   { $$ = AddToList($1, $3); }
 ;

asmexpr:
  conditional_expression
    { $$ = $1; }
  | '\\' conditional_expression
    { $$ = AstCatch($2); }
;

instruction:
  C_INSTR
  { $$ = $1; }
  | instrmodifier instruction
  { $$ = AddToList($2, $1); }
;
 
instrmodifier:
  C_INSTRMODIFIER
  { $$ = $1; }
;

modifierlist:
  instrmodifier
    { $$ = $1; }
  | modifierlist instrmodifier
    { $$ = AddToList($1, $2); }
  | modifierlist ',' instrmodifier
    { $$ = AddToList($1, $3); }
  ;
  
func_declaration_list
	: declaration
            { $$ = $1; }
	| declaration_list declaration
            { $$ = AddToList($1, $2); }
	;

declaration_list
	: declaration
            { $$ = MakeDeclarations($1, currentTypes); }
	| declaration_list declaration
            { $$ = AddToList($1, MakeDeclarations($2, currentTypes)); }
	;

expression_statement
	: ';'
            { $$ = NULL; }
	| expression ';'
            { $$ = NewAST(AST_STMTLIST, $1, NULL); }
	;

selection_statement
	: C_IF '(' expression ')' statement
            { $$ = NewAST(AST_IF, $3,
                          NewAST(AST_THENELSE, ForceStatementList($5), NULL)); }
	| C_IF '(' expression ')' statement C_ELSE statement
            { $$ = NewAST(AST_IF, $3,
                          NewAST(AST_THENELSE, ForceStatementList($5), ForceStatementList($7)));
            }
	| C_SWITCH '(' expression ')' statement
            {
                $$ = NewCommentedAST(AST_CASE, $3, $5, $1);
            }
	;

iteration_statement
	: C_WHILE '(' expression ')' statement
            { AST *body = ForceStatementList(CheckYield($5));
              $$ = NewCommentedAST(AST_WHILE, $3, body, $1);
            }
	| C_DO statement C_WHILE '(' expression ')' ';'
            { AST *body = ForceStatementList(CheckYield($2));
              $$ = NewCommentedAST(AST_DOWHILE, $5, body, $1);
            }
	| for_statement_start '(' expression_statement expression_statement ')' statement
            {   AST *body = ForceStatementList(CheckYield($6));
                AST *init = $3;
                AST *cond = $4;
                AST *update = NULL;
                AST *stepstmt, *condtest;
                if (init && init->kind == AST_STMTLIST && !init->right) {
                    init = init->left;
                }
                if (cond && cond->kind == AST_STMTLIST && !cond->right) {
                    cond = cond->left;
                }
                stepstmt = NewAST(AST_STEP, update, body);
                condtest = NewAST(AST_TO, cond, stepstmt);
                $$ = NewCommentedAST(AST_FOR, init, condtest, $1);
                PopCurrentTypes();
            }
	| for_statement_start '(' expression_statement expression_statement expression ')' statement
            {   AST *body = ForceStatementList(CheckYield($7));
                AST *init = $3;
                AST *cond = $4;
                AST *update = $5;
                AST *stepstmt, *condtest;
                if (init && init->kind == AST_STMTLIST && !init->right) {
                    init = init->left;
                }
                if (cond && cond->kind == AST_STMTLIST && !cond->right) {
                    cond = cond->left;
                }
                stepstmt = NewAST(AST_STEP, update, body);
                condtest = NewAST(AST_TO, cond, stepstmt);
                $$ = NewCommentedAST(AST_FOR, init, condtest, $1);
                PopCurrentTypes();
            }
	| for_statement_start '(' for_declaration expression_statement ')' statement
            {   AST *body = ForceStatementList(CheckYield($6));
                AST *init = $3;
                AST *cond = $4;
                AST *update = NULL;
                AST *stepstmt, *condtest;
                if (init && init->kind == AST_STMTLIST && !init->right) {
                    init = init->left;
                }
                if (cond && cond->kind == AST_STMTLIST && !cond->right) {
                    cond = cond->left;
                }
                stepstmt = NewAST(AST_STEP, update, body);
                condtest = NewAST(AST_TO, cond, stepstmt);
                body = NewCommentedAST(AST_FOR, NULL, condtest, $1);
                init = NewAST(AST_STMTLIST, init,
                              NewAST(AST_STMTLIST, body, NULL));
                $$ = NewAST(AST_SCOPE, init, NULL);
                PopCurrentTypes();
            }
	| for_statement_start '(' for_declaration expression_statement expression ')' statement
            {   AST *body = ForceStatementList(CheckYield($7));
                AST *init = $3;
                AST *cond = $4;
                AST *update = $5;
                AST *stepstmt, *condtest;
                if (init && init->kind == AST_STMTLIST && !init->right) {
                    init = init->left;
                }
                if (cond && cond->kind == AST_STMTLIST && !cond->right) {
                    cond = cond->left;
                }
                stepstmt = NewAST(AST_STEP, update, body);
                condtest = NewAST(AST_TO, cond, stepstmt);
                body = NewCommentedAST(AST_FOR, NULL, condtest, $1);
                init = NewAST(AST_STMTLIST, init,
                              NewAST(AST_STMTLIST, body, NULL));
                $$ = NewAST(AST_SCOPE, init, NULL);
                PopCurrentTypes();
            }
	;
for_declaration
	: declaration
          { $$ = MakeDeclarations($1, currentTypes); }
	;

jump_statement
	: C_GOTO any_identifier ';'
            { $$ = NewCommentedAST(AST_GOTO, LabelName($2), NULL, $1); }
	| C_CONTINUE ';'
            { $$ = NewCommentedAST(AST_CONTINUE, NULL, NULL, $1); }
	| C_BREAK ';'
            { $$ = NewCommentedAST(AST_ENDCASE, NULL, NULL, $1); }
	| C_RETURN ';'
            { $$ = NewCommentedAST(AST_RETURN, NULL, NULL, $1); }
	| C_RETURN expression ';'
            { $$ = NewCommentedAST(AST_RETURN, $2, NULL, $1); }
	| C_THROW expression ';'
            { $$ = NewCommentedAST(AST_THROW, $2, NULL, $1); }
	;

translation_unit
	: external_declaration
	| translation_unit external_declaration
	;

external_declaration
	: function_definition
        | top_asm
        | top_pasm
	| declaration
           { DeclareCGlobalVariables($1); }
	;

function_definition
	: declaration_specifiers declarator func_declaration_list compound_statement
            {
                AST *type = $1;
                AST *decl = $2;
                AST *decl_list = $3;
                AST *body = $4;
                AST *ident;
                int is_public = 1;

                decl = MergeOldStyleDeclarationList(decl, decl_list);
                type = CombineTypes(type, decl, &ident);
                DeclareCTypedFunction(current, type, ident, is_public, body, NULL);
            }
	| declaration_specifiers declarator attribute_decl compound_statement
            {
                AST *type = $1;
                AST *decl = $2;
                AST *anno = $3;
                AST *body = $4;
                AST *ident;
                int is_public = 1;
                type = CombineTypes(type, decl, &ident);
                DeclareCTypedFunction(current, type, ident, is_public, body, anno);
            }
	| declarator func_declaration_list compound_statement
            {
                AST *type;
                AST *ident;
                AST *decl = $1;
                AST *decl_list = $2;
                AST *body = $3;
                int is_public = 1;
                decl = MergeOldStyleDeclarationList(decl, decl_list);
                type = CombineTypes(NULL, decl, &ident);
                DeclareCTypedFunction(current, type, ident, is_public, body, NULL);
            }
	| declarator attribute_decl compound_statement
            {
                AST *type;
                AST *ident;
                AST *decl = $1;
                AST *anno = $2;
                AST *body = $3;
                int is_public = 1;
                type = CombineTypes(NULL, decl, &ident);
                DeclareCTypedFunction(current, type, ident, is_public, body, anno);
            }
	| declaration_specifiers declarator fromfile_decl
            {
                AST *ident;
                AST *decl_list = $1;
                AST *type = $2;
                AST *body = $3;
                int is_public = 1;
                type = CombineTypes(decl_list, type, &ident);
                DeclareCTypedFunction(current, type, ident, is_public, body, NULL);
            }
	| declarator fromfile_decl
            {
                AST *type = $1;
                AST *ident;
                AST *body = $2;
                int is_public = 1;
                type = CombineTypes(NULL, type, &ident);
                DeclareCTypedFunction(current, type, ident, is_public, body, NULL);
            }
	;

attribute_decl
        : C_ATTRIBUTE
            {
                $$ = $1;
            }
         | /* empty */
            { $$ = NULL; }
        ;

fromfile_decl
        : C_FROMFILE '(' C_STRING_LITERAL ')' ';'
            {
                AST *str = $3;
                if (str && str->kind == AST_EXPRLIST) {
                    str = str->left;
                }
                $$ = str;
            }
        ;

/* PASM syntax: this is awkward, so not fully supported yet */
top_pasm:
  C_PASM '{' pasmlist '}'
      { $$ = current->datblock = AddToListEx(current->datblock, $3, &current->datblock_tail); }
  | C_PASM C_EOLN '{' pasmlist '}'
      { $$ = current->datblock = AddToListEx(current->datblock, $4, &current->datblock_tail); }
;

pasmlist:
  pasmline
  { $$ = $1; }
  | pasmlist pasmline
  { $$ = AddToList($1, $2); }
  ;

pasmline:
  pasm_baseline
  | C_IDENTIFIER pasm_baseline
    {   AST *linebreak;
        AST *comment = GetComments();
        AST *ast;
        ast = $1;
        if (comment && (comment->d.string || comment->kind == AST_SRCCOMMENT)) {
            linebreak = NewCommentedAST(AST_LINEBREAK, NULL, NULL, comment);
        } else {
            linebreak = NewAST(AST_LINEBREAK, NULL, NULL);
        }
        ast = AddToList(ast, $2);
        ast = AddToList(linebreak, ast);
        $$ = ast;
    }
  ;

pasm_baseline:
  C_EOLN
    { $$ = NULL; }
  | error C_EOLN
    { $$ = NULL; }
  | C_BYTE C_EOLN
    { $$ = NewCommentedAST(AST_BYTELIST, NULL, NULL, $1); }
  | C_BYTE pasm_operandlist C_EOLN
    { $$ = NewCommentedAST(AST_BYTELIST, $2, NULL, $1); }
  | C_WORD C_EOLN
    { $$ = NewCommentedAST(AST_WORDLIST, NULL, NULL, $1); }
  | C_WORD pasm_operandlist C_EOLN
    { $$ = NewCommentedAST(AST_WORDLIST, $2, NULL, $1); }
  | C_LONG C_EOLN
    { $$ = NewCommentedAST(AST_LONGLIST, NULL, NULL, $1); }
  | C_LONG pasm_operandlist C_EOLN
    { $$ = NewCommentedAST(AST_LONGLIST, $2, NULL, $1); }
  | instruction C_EOLN
    { $$ = NewCommentedInstr($1); }
  | instruction pasm_operandlist C_EOLN
    { $$ = NewCommentedInstr(AddToList($1, $2)); }
  | instruction modifierlist C_EOLN
    { $$ = NewCommentedInstr(AddToList($1, $2)); }
  | instruction pasm_operandlist modifierlist C_EOLN
    { $$ = NewCommentedInstr(AddToList($1, AddToList($2, $3))); }
  | C_ALIGNL C_EOLN
    { $$ = NewCommentedAST(AST_ALIGN, AstInteger(4), NULL, $1); }
  | C_ALIGNW C_EOLN
    { $$ = NewCommentedAST(AST_ALIGN, AstInteger(2), NULL, $1); }
  | C_ORG C_EOLN
    { $$ = NewCommentedAST(AST_ORG, NULL, NULL, $1); }
  | C_ORG pasmexpr C_EOLN
    { $$ = NewCommentedAST(AST_ORG, $2, NULL, $1); }
  | C_ORGH C_EOLN
    { $$ = NewCommentedAST(AST_ORGH, NULL, NULL, $1); }
  | C_ORGH pasmexpr C_EOLN
    { $$ = NewCommentedAST(AST_ORGH, $2, NULL, $1); }
  | C_ORGF pasmexpr C_EOLN
    { $$ = NewCommentedAST(AST_ORGF, $2, NULL, $1); }
  | C_RES pasmexpr C_EOLN
    { $$ = NewCommentedAST(AST_RES, $2, NULL, $1); }
  | C_FIT pasmexpr C_EOLN
    { $$ = NewCommentedAST(AST_FIT, $2, NULL, $1); }
  | C_FIT C_EOLN
    { $$ = NewCommentedAST(AST_FIT, AstInteger(0x1f0), NULL, $1); }
  | C_FILE C_STRING_LITERAL C_EOLN
    { $$ = NewCommentedAST(AST_FILE, GetFullFileName($2), NULL, $1); }
  ;

pasm_operand:
  pasmexpr
   { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
 | '#' pasmexpr
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_IMMHOLDER, $2, NULL), NULL); }
 | '#' '#' pasmexpr
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_BIGIMMHOLDER, $3, NULL), NULL); }
 | pasmexpr '[' pasmexpr ']'
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_ARRAYREF, $1, $3), NULL); }
;

pasm_operandlist:
   pasm_operand
   { $$ = $1; }
 | pasm_operandlist ',' pasm_operand
   { $$ = AddToList($1, $3); }
 ;

pasmatom
      : C_IDENTIFIER
            { $$ = $1; }
      | C_CONSTANT
            { $$ = $1; }
      | C_HWREG
            { $$ = $1; }
      | C_STRING_LITERAL
            { $$ = $1; }
      | '@' C_IDENTIFIER
            { $$ = NewAST(AST_ADDROF, $2, NULL); }
      | '(' pasmexpr ')'
            { $$ = $2; }
      | pasmatom C_INC_OP
            { $$ = AstOperator(K_INCREMENT, $1, NULL); }
      | pasmatom C_DEC_OP
            { $$ = AstOperator(K_DECREMENT, $1, NULL); }
;

pasmunary
      : pasmatom
          { $$ = $1; }
      | '-' pasmunary
          { $$ = AstOperator(K_NEGATE, NULL, $2); }
      | '!' pasmunary
          { $$ = AstOperator(K_BIT_NOT, NULL, $2); }
;

pasm_e1
      : pasmunary
            { $$ = $1; }
      | pasm_e1 C_LEFT_OP pasmunary
            { $$ = AstOperator(K_SHL, $1, $3); } 
      | pasm_e1 C_RIGHT_OP pasmunary
            { $$ = AstOperator(K_SHR, $1, $3); } 
;
pasm_e2
      : pasm_e1
            { $$ = $1; }
      | pasm_e2 '&' pasm_e1
            { $$ = AstOperator('&', $1, $3); } 
;
pasm_e3
      : pasm_e2
            { $$ = $1; }
      | pasm_e3 '|' pasm_e2
            { $$ = AstOperator('|', $1, $3); } 
      | pasm_e3 '^' pasm_e2
            { $$ = AstOperator('^', $1, $3); } 
;
pasm_e4
      : pasm_e3
            { $$ = $1; }
      | pasm_e4 '*' pasm_e3
            { $$ = AstOperator('*', $1, $3); } 
      | pasm_e4 '/' pasm_e3
            { $$ = AstOperator('/', $1, $3); } 
      | pasm_e4 '/' '/' pasm_e3
            { $$ = AstOperator(K_MODULUS, $1, $4); } 
;
pasm_e5
      : pasm_e4
            { $$ = $1; }
      | pasm_e5 '+' pasm_e4
            { $$ = AstOperator('+', $1, $3); } 
      | pasm_e5 '-' pasm_e4
            { $$ = AstOperator('-', $1, $3); } 
;
pasm_e6
      : pasm_e5
            { $$ = $1; }
      | pasm_e6 C_LIMITMIN_OP pasm_e5
            { $$ = AstOperator(K_LIMITMIN, $1, $3); } 
      | pasm_e6 C_LIMITMAX_OP pasm_e5
            { $$ = AstOperator(K_LIMITMAX, $1, $3); } 
;
pasm_e7
      : pasm_e6
            { $$ = $1; }
      | pasm_e7 '>' pasm_e6
            { $$ = AstOperator('>', $1, $3); } 
      | pasm_e7 '<' pasm_e6
            { $$ = AstOperator('<', $1, $3); } 
      | pasm_e7 C_LE_OP pasm_e6
            { $$ = AstOperator(K_LE, $1, $3); } 
      | pasm_e7 C_GE_OP pasm_e6
            { $$ = AstOperator(K_GE, $1, $3); } 
;

pasmexpr
      : pasm_e7
            { $$ = $1; }
      | '\\' pasmexpr
            { $$ = AstCatch($2); }
;

%%
#include <stdio.h>

void
cgramyyerror(const char *msg)
{
    extern int saved_cgramyychar;
    int yychar = saved_cgramyychar;
    
    ERRORHEADER(current->Lptr->fileName, current->Lptr->lineCounter, "error");

    // massage bison's error messages to make them easier to understand
    while (*msg) {
        // say which identifier was unexpected
        if (!strncmp(msg, "unexpected identifier", strlen("unexpected identifier")) && last_ast && last_ast->kind == AST_IDENTIFIER) {
            fprintf(stderr, "unexpected identifier `%s'", last_ast->d.string);
            msg += strlen("unexpected identifier");
        }
        else if (!strncmp(msg, "unexpected type name", strlen("unexpected type name")) && last_ast && last_ast->kind == AST_IDENTIFIER) {
            fprintf(stderr, "unexpected type name `%s'", last_ast->d.string);
            msg += strlen("unexpected type name");
        }
        // if we get a stray character in source, sometimes bison tries to treat it as a token for
        // error purposes, resulting in $undefined as the token
        else if (!strncmp(msg, "$undefined", strlen("$undefined")) && yychar >= ' ' && yychar < 127) {
            fprintf(stderr, "%c", yychar);
            msg += strlen("$undefined");
        }
        else {
            fprintf(stderr, "%c", *msg);
            msg++;
        }
    }
    fprintf(stderr, "\n");
    gl_errors++;
}

