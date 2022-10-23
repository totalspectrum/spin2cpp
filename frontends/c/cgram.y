/*
 * C compiler parser
 * Copyright (c) 2011-2022 Total Spectrum Software Inc.
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

    extern AST *BuildDebugList(AST *); /* in spin.y */
    
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
    // and that further lengthening it will make it 8 bytes
    return ast_type_c_long;
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

static AST *CombineTypes(AST *first, AST *second, AST **identifier, Module **module);

static AST *
C_ModifySignedUnsigned(AST *modifier, AST *type)
{
    if (modifier == ast_type_c_signed) {
        type = MakeSigned(type, 1);
    } else if (modifier == ast_type_unsigned_long) {
        type = MakeSigned(type, 0);
    } else if (modifier == ast_type_c_long) {
    // we need to be able to distinguish between
    // "long long" and "long int"
        type = LengthenType(type);
    } else {
        type = CombineTypes(modifier, type, NULL, NULL);
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

Module *
FindClassByAst(AST *astName)
{
    const char *name = GetUserIdentifierName(astName);
    Module *P = NULL;
    Symbol *sym = LookupSymbolInTable(currentTypes, name);
    if (sym && sym->kind == SYM_TYPEDEF) {
        AST *typ = (AST *)sym->val;
        if (typ->kind != AST_OBJECT) {
            ERROR(astName, "%s is not a class", name);
            return NULL;
        }
        P = GetClassPtr(typ);
    }
    if (!P) {
        ERROR(astName, "cannot find class %s", name);
    }
    return P;
}

static AST *MergePrefix(AST *prefix, AST *first)
{
    if (prefix) {
        // make sure we migrate STATIC, REGISTER, and EXTERN to the front of the list
        if (first && first->kind == AST_REGISTER) {
            first = first->left;
            prefix = AddToLeftList(prefix, NewAST(AST_REGISTER, NULL, NULL));
        }
        if (first && (first->kind == AST_STATIC || first->kind == AST_EXTERN || first->kind == AST_REGISTER)) {
            first->left = AddToLeftList(prefix, first->left);
        } else {
            first = AddToLeftList(prefix, first);
        }
    }
    return first;
}

static AST *
CombineTypes(AST *first, AST *second, AST **identifier, Module **module)
{
    AST *expr, *ident;
    AST *prefix = NULL;
    
    if (second && second->kind == AST_COMMENT) {
        second = NULL;
    }
    if (first && first->kind == AST_COMMENT) {
        first = NULL;
    }
    if (!second || first == second) {
        return first;
    }
    while (first && (first->kind == AST_STATIC || first->kind == AST_TYPEDEF || first->kind == AST_EXTERN || first->kind == AST_REGISTER)) {
        AST *dup = DupAST(first);
        first = first->left;
        dup->left = NULL;
        if (prefix) {
            prefix->left = dup;
        } else {
            prefix = dup;
        }
    }
    switch (second->kind) {
    case AST_DECLARE_VAR:
        first = CombineTypes(first, second->left, identifier, module);
        first = CombineTypes(first, second->right, identifier, module);
        return MergePrefix(prefix, first);
    case AST_METHODREF:
        if (module) {
            if (*module) {
                ERROR(second, "flexspin only supports one :: in declarations");
            } else {
                *module = FindClassByAst(second->left);
            }
            second = second->right;
        }
        /* fall through */
    case AST_SYMBOL:
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER:
        if (identifier) {
            *identifier = second;
        }
        return MergePrefix(prefix, first);
    case AST_ARRAYDECL:
    case AST_ARRAYTYPE:
        first = NewAST(AST_ARRAYTYPE, first, second->right);
        return MergePrefix(prefix, CombineTypes(first, second->left, identifier, module));
    case AST_REFTYPE:
    case AST_PTRTYPE:
        if (identifier || !IsFunctionType(second->left)) {
            first = NewAST(second->kind, first, NULL);
        }
        second = CombineTypes(first, second->left, identifier, module);
        return MergePrefix(prefix, second);
        
    case AST_FUNCTYPE:
        second->left = CombineTypes(first, second->left, identifier,  module);
        return MergePrefix(prefix, second);
    case AST_ASSIGN:
        expr = second->right;
        first = CombineTypes(first, second->left, &ident, module);
        ident = AstAssign(ident, expr);
        if (identifier) {
            *identifier = ident;
        }
        return MergePrefix(prefix, first);
    case AST_MODIFIER_CONST:
    case AST_MODIFIER_VOLATILE:
        expr = NewAST(second->kind, NULL, NULL);
        second = MergePrefix(prefix, CombineTypes(first, second->left, identifier, module));
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
        if (second && second == ast_type_long) {
            return first;
        }
        ERROR(first ? first : second, "Unable to combine types");
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
    int inSubModule = !IsTopLevel(P);
    
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
        if (nameAst->kind == AST_LOCAL_IDENTIFIER || inSubModule) {
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

static AST *IsGlobalRegisterDecl(AST *type)
{
    if (!type || !type->left) return NULL;
    
    if ( (type->kind == AST_EXTERN && type->left->kind == AST_REGISTER)
         || (type->kind == AST_REGISTER && type->left->kind == AST_EXTERN) )
    {
        type = type->left->left;
        return type;
    }
    return NULL;
}

static AST *
MultipleDeclareVar(AST *first, AST *second)
{
    AST *ident, *type;
    AST *stmtlist = NULL;
    AST *item;
    Module *module = NULL;
    AST *regtype = NULL;
    
    while (second) {
        if (second->kind != AST_LISTHOLDER) {
            ERROR(second, "internal error in createVarDeclarations: expected listholder");
            return stmtlist;
        }
        item = second->left;
        second = second->right;
        type = CombineTypes(first, item, &ident, &module);
        if (module) {
            ERROR(first, ":: not supported yet");
        }
        if (type && type->kind == AST_STATIC) {
            stmtlist = AddToList(stmtlist, DeclareStatics(current, type->left, ident));
        } else if ( NULL != (regtype = IsGlobalRegisterDecl(type)) ) {
            /* declare a register global variable */
            ident = NewAST(AST_DECLARE_VAR, regtype, ident);
            DeclareTypedRegisterVariables(ident);
        } else if (type && (type->kind == AST_EXTERN || IsConstArrayType(type)) ) {
            if (type->kind == AST_EXTERN) type = type->left;
            /* declare a global variable */
            ident = NewAST(AST_DECLARE_VAR, type, ident);
            DeclareTypedGlobalVariables(ident, 1); // "extern" variables are always in DAT
        } else {
            while (type && type->kind == AST_REGISTER) {
                type = type->left;
            }
            ident = NewAST(AST_DECLARE_VAR, type, ident);
            stmtlist = AddToList(stmtlist, NewAST(AST_STMTLIST, ident, NULL));
        }
    }
    return stmtlist;
}

AST *
SingleDeclareInitedVar(AST *decl_spec, AST *declarator, AST *initval)
{
    AST *type, *ident;
    Module *module = NULL;
    
    ident = NULL;
    type = CombineTypes(decl_spec, declarator, &ident, &module);
    if (type && type->kind == AST_EXTERN) {
        // just ignore EXTERN declarations
        return NULL;
    }
    if (!ident && !type) {
        return NULL;
    }
    if (module) {
        ERROR(declarator, ":: not supported for variable declarations yet");
    }
    while (type && type->kind == AST_REGISTER) {
        type = type->left;
    }
    if (initval) {
        ident = NewAST(AST_ASSIGN, ident, initval);
    }
    return NewAST(AST_DECLARE_VAR, type, ident);
}

AST *
SingleDeclareVar(AST *decl_spec, AST *declarator)
{
    return SingleDeclareInitedVar(decl_spec, declarator, NULL);
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
        inDat = current->fromUsing ? 0 : 1;
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
    P->conblock = NULL;
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
    int is_private = -P->defaultPrivate;
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
        if (ast->kind == AST_PUBFUNC) {
            is_private = 0;
            continue;
        }
        if (ast->kind == AST_PRIFUNC) {
            is_private = 1;
            continue;
        }
        if (is_private == -1) {
            WARNING(ast, "declarations in class with no explicit public or private; assuming private");
            is_private = 1;
        }
        if (ast->kind == AST_FUNCDECL) {
            AST *type;
            AST *ident;
            AST *body;
            int is_public = !is_private;
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
                Symbol *sym = FindSymbol(&P->objsyms, GetIdentifierName(ident));

                if (sym && sym->kind == SYM_ALIAS) {
                    goto skip_decl;
                }
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
            skip_decl:
                ;
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
        ERROR(skind, "Internal error, not struct or union");
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
        ERROR(identifier, "Internal error, bad struct def");
        return NULL;
    }
    name = GetIdentifierName(identifier);
    typname = (char *)malloc(strlen(name)+strlen(classname)+16);
    if (LangStructAutoTypedef(current->curLanguage)) {
        strcpy(typname, name);
    } else {
        strcpy(typname, classname);
        strcat(typname, "_struct__");
        strcat(typname, name);
    }
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
            class_type = NewAbstractObject(AstIdentifier(typname), body, 1);
            Parent->objblock = AddToList(Parent->objblock, class_type);
            body = NULL;
            C = NULL;
        } else {
            C = NewModule(typname, LANG_CFAMILY_C);
            C->defaultPrivate = is_class;
            C->Lptr = current->Lptr;
            C->isUnion = is_union;
            class_type = NewAbstractObject(AstIdentifier(typname), NULL, 0);
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
    if (ftype && ftype->kind == AST_EXTERN) {
        ftype = ftype->left;
    }
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
        ERROR(decl, "Internal error, expected variable declaration");
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
%token C_TYPEOF     "typeof"

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
%token C_DOUBLECOLON "::"

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
%token C_DEBUG "__debug"
%token C_INSTR "asm instruction"
%token C_INSTRMODIFIER "instruction modifier"
%token C_HWREG "hardware register"

// C++ tokens
%token C_CATCH "catch"
%token C_CLASS "class"
%token C_DELETE "delete"
%token C_FALSE "false"
%token C_NEW "new"
%token C_NULLPTR "nullptr"
%token C_TRUE "true"
%token C_PRIVATE "private"
%token C_PUBLIC "public"
%token C_TEMPLATE "template"
%token C_THIS "this"
%token C_THROW "throw"
%token C_THROWIF "__throwifcaught"
%token C_TRY "try"

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

%token C_FUNC "__func__ or __FUNCTION__"

// builtin functions
%token C_BUILTIN_ABS    "__builtin_abs"
%token C_BUILTIN_CLZ    "__builtin_clz"
%token C_BUILTIN_SQRT   "__builtin_sqrt"
%token C_BUILTIN_FRAC   "__builtin_frac"
%token C_BUILTIN_MULH   "__builtin_mulh"
%token C_BUILTIN_MULUH  "__builtin_muluh"

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
        | C_TRUE
            { $$ = AstInteger(1); }
        | C_FALSE
            { $$ = AstInteger(0); }
        | C_NULLPTR
            { $$ = AstBitValue(0); }
	| C_STRING_LITERAL
            { $$ = NewAST(AST_STRINGPTR, $1, NULL); }
	| C_FUNC
            { $$ = NewAST(AST_FUNC_NAME, NULL, NULL); }
        | C_BUILTIN_PRINTF
            { $$ = NewAST(AST_PRINT, NULL, NULL); }
	| '$'
            { $$ = NewAST(AST_HERE, NULL, NULL); }
	| '(' expression ')'
            { $$ = $2; }
	| '(' compound_statement_open block_item_list compound_statement_close ')'
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
        | C_BUILTIN_FRAC '(' argument_expression_list ')'
            {
                AST *list;
                AST *arg1, *arg2;
                
                list = $3;
                if (!list || !list->left)
                {
                    SYNTAX_ERROR("Missing argument to __builtin_frac");
                    arg1 = AstInteger(1);
                } else {
                    arg1 = list->left;
                }
                if (list && list->right) {
                    arg2 = list->right->left;
                    if (list->right->right) {
                        SYNTAX_ERROR("Too many arguments to __builtin_frac");
                    }
                } else {
                    arg2 = AstInteger(0);
                }
                $$ = AstOperator(K_FRAC64, arg1, arg2);
            }
        | C_BUILTIN_MULUH '(' argument_expression_list ')'
            {
                AST *list;
                AST *arg1, *arg2;
                
                list = $3;
                if (!list || !list->left)
                {
                    SYNTAX_ERROR("Missing argument to __builtin_muluh");
                    arg1 = AstInteger(1);
                } else {
                    arg1 = list->left;
                }
                if (list && list->right) {
                    arg2 = list->right->left;
                    if (list->right->right) {
                        SYNTAX_ERROR("Too many arguments to __builtin_muluh");
                    }
                } else {
                    arg2 = AstInteger(0);
                }
                $$ = AstOperator(K_UNS_HIGHMULT, arg1, arg2);
            }
        | C_BUILTIN_MULH '(' argument_expression_list ')'
            {
                AST *list;
                AST *arg1, *arg2;
                
                list = $3;
                if (!list || !list->left)
                {
                    SYNTAX_ERROR("Missing argument to __builtin_mulh");
                    arg1 = AstInteger(1);
                } else {
                    arg1 = list->left;
                }
                if (list && list->right) {
                    arg2 = list->right->left;
                    if (list->right->right) {
                        SYNTAX_ERROR("Too many arguments to __builtin_mulh");
                    }
                } else {
                    arg2 = AstInteger(0);
                }
                $$ = AstOperator(K_HIGHMULT, arg1, arg2);
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

                decl = SingleDeclareInitedVar(typ, id, initlist);
                stmt = NewAST(AST_STMTLIST, decl, NULL);                
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
            {
                AST *typ = $2;
                AST *expr = $4;
                $$ = NewAST(AST_CAST, typ, expr);
            }
/*
	| type_name '(' unary_expression ')'
            {
                LANGUAGE_WARNING(LANG_CFAMILY_C, NULL, "Allowing C++ style casts in C code is a FlexC extension");
                $$ = NewAST(AST_CAST, $1, $3);
            }
*/
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
            { $$ = NewAST(AST_REGISTER, NULL, NULL); }
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
        | C_TYPEOF '(' unary_expression ')'
            {
                AST *typ = ExprType($2);
                if (!typ) {
                    WARNING($1, "Unable to find type of expression, assuming generic");
                    typ = NULL;
                }
                $$ = typ;
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
        : struct_specifier_qualifier_list ';' /* for anonymous struct/union */
            {
                AST *dummy;
                AST *anonstruct = $1;
                AST *decl;
                dummy = AstTempIdentifier("__anonymous__");
                dummy = NewAST(AST_LISTHOLDER, dummy, NULL);
                decl = MultipleDeclareVar(anonstruct, dummy);
                // declare aliases for each variable in the object
                Module *P = GetClassPtr(anonstruct);
                if (P) {
                    DeclareAnonymousAliases(current, P, dummy);
                }
                $$ = decl;
            }
	| struct_specifier_qualifier_list struct_declarator_list ';'
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
        | struct_specifier_qualifier_list struct_declarator_list compound_statement
            {
                AST *type;
                AST *ident = NULL;
                AST *body = $3;
                AST *decl = $2;
                AST *spqual = $1;
                AST *top_decl;
                Module *module = NULL;
                
                if (decl->right) {
                    SYNTAX_ERROR("bad method declaration");
                }
                type = CombineTypes(spqual, decl->left, &ident, &module);

                top_decl = NewAST(AST_LISTHOLDER, type,
                                  NewAST(AST_LISTHOLDER, ident,
                                         NewAST(AST_LISTHOLDER, body, NULL)));
                if (module) {
                    ERROR(decl, ":: not supported inside structures");
                }
                top_decl = NewAST(AST_FUNCDECL, top_decl, NULL);
                $$ = NewAST(AST_STMTLIST, top_decl, NULL);
            }        
	;

struct_specifier_qualifier_list
	: type_specifier specifier_qualifier_list
            { $$ = C_ModifySignedUnsigned($1, $2); }
	| type_specifier
            { $$ = $1; }
	| type_qualifier struct_specifier_qualifier_list
            { $$ = $1; $$->left = $2; }
	| type_qualifier
            { $$ = $1; $$->left = ast_type_long; }
	| C_STATIC struct_specifier_qualifier_list
            {
                AST *list = $2;
                AST *static_spec = NewAST(AST_STATIC, NULL, NULL);
                $$ = MergePrefix(static_spec, list);
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
        | C_IDENTIFIER C_DOUBLECOLON C_IDENTIFIER
            {
                AST *modname = $1;
                AST *member = $3;
                $$ = NewAST(AST_METHODREF, modname, member);
            }
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
            {
                AST *lhs = $1;
                AST *rhs = $2;
                AST *final = CombineTypes(lhs, rhs, NULL, NULL);
                $$ = final;
            }
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
            { $$ = $2; }
	| '[' ']'
            { $$ = NewAST(AST_ARRAYTYPE, NULL, NULL); }
	| '[' constant_expression ']'
            { $$ = NewAST(AST_ARRAYTYPE, NULL, $2); }
	| direct_abstract_declarator '[' ']'
            { $$ = NewAST(AST_ARRAYTYPE, $1, NULL); }
	| direct_abstract_declarator '[' constant_expression ']'
            { $$ = NewAST(AST_ARRAYTYPE, $1, $3); }
	| '(' ')'
            {
                $$ = NULL;
            }
	| '(' parameter_type_list ')'
            {
                AST *parmlist = ProcessParamList($2);
                $$ = NewAST(AST_FUNCTYPE, ast_type_long, parmlist);
            }
	| direct_abstract_declarator '(' ')'
            {
                AST *rettype = $1;
                AST *parmlist = NULL;
                AST *ftype;
                bool need_pointer = false;
                if (IsPointerType(rettype)) {
                    need_pointer = true;
                    rettype = BaseType(rettype);
                }
                ftype = NewAST(AST_FUNCTYPE, rettype, parmlist);
                if (need_pointer) {
                    ftype = NewAST(AST_PTRTYPE, ftype, NULL);
                }
                $$ = ftype;
            }
	| direct_abstract_declarator '(' parameter_type_list ')'
            {
                AST *rettype = $1;
                AST *parmlist = ProcessParamList($3);
                AST *ftype;
                bool need_pointer = false;
                if (IsPointerType(rettype)) {
                    need_pointer = true;
                    rettype = BaseType(rettype);
                }
                ftype = NewAST(AST_FUNCTYPE, rettype, parmlist);
                if (need_pointer) {
                    ftype = NewAST(AST_PTRTYPE, ftype, NULL);
                }
                $$ = ftype;
            }
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
        | try_statement
	;

labeled_statement
	: any_identifier ':' statement
            {
                AST *label = NewAST(AST_LABEL, LabelName($1), NULL);
                $$ = NewAST(AST_STMTLIST, label,
                              NewAST(AST_STMTLIST, $3, NULL));
            }
	| C_CASE constant_expression C_ELLIPSIS constant_expression ':' statement
            {
                AST *stmt = $6;
                AST *range = NewAST(AST_RANGE, $2, $4);
                range = NewAST(AST_EXPRLIST, range, NULL);
                $$ = NewAST(AST_CASEITEM, range, stmt);
            }
	| C_CASE constant_expression ':' statement
            {
                AST *range = $2;
                AST *stmt = $4;
                range = NewAST(AST_EXPRLIST, range, NULL);
                $$ = NewAST(AST_CASEITEM, range, stmt);
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

asm_eoln:
  C_EOLN
  | ';'
;

asm_baseline:
  asm_eoln
    { $$ = NULL; }
  | error asm_eoln
    { $$ = NULL; }
  | C_BYTE asm_eoln
    { $$ = NewCommentedAST(AST_BYTELIST, NULL, NULL, $1); }
  | C_BYTE asm_operandlist asm_eoln
    { $$ = NewCommentedAST(AST_BYTELIST, $2, NULL, $1); }
  | C_WORD asm_eoln
    { $$ = NewCommentedAST(AST_WORDLIST, NULL, NULL, $1); }
  | C_WORD asm_operandlist asm_eoln
    { $$ = NewCommentedAST(AST_WORDLIST, $2, NULL, $1); }
  | C_LONG asm_eoln
    { $$ = NewCommentedAST(AST_LONGLIST, NULL, NULL, $1); }
  | C_LONG asm_operandlist asm_eoln
    { $$ = NewCommentedAST(AST_LONGLIST, $2, NULL, $1); }
  | instruction asm_eoln
    { $$ = NewCommentedInstr($1); }
  | instruction asm_operandlist asm_eoln
    { $$ = NewCommentedInstr(AddToList($1, $2)); }
  | instruction modifierlist asm_eoln
    { $$ = NewCommentedInstr(AddToList($1, $2)); }
  | instruction asm_operandlist modifierlist asm_eoln
    { $$ = NewCommentedInstr(AddToList($1, AddToList($2, $3))); }
  | C_ALIGNL asm_eoln
    { $$ = NewCommentedAST(AST_ALIGN, AstInteger(4), NULL, $1); }
  | C_ALIGNW asm_eoln
    { $$ = NewCommentedAST(AST_ALIGN, AstInteger(2), NULL, $1); }
  | C_ORG asm_eoln
    { $$ = NewCommentedAST(AST_ORG, NULL, NULL, $1); }
  | C_ORG asmexpr asm_eoln
    { $$ = NewCommentedAST(AST_ORG, $2, NULL, $1); }
  | C_ORGH asm_eoln
    { $$ = NewCommentedAST(AST_ORGH, NULL, NULL, $1); }
  | C_ORGH asmexpr asm_eoln
    { $$ = NewCommentedAST(AST_ORGH, $2, NULL, $1); }
  | C_ORGF asmexpr asm_eoln
    { $$ = NewCommentedAST(AST_ORGF, $2, NULL, $1); }
  | C_RES asmexpr asm_eoln
    { $$ = NewCommentedAST(AST_RES, $2, NULL, $1); }
  | C_FIT asmexpr asm_eoln
    { $$ = NewCommentedAST(AST_FIT, $2, NULL, $1); }
  | C_FIT asm_eoln
    { $$ = NewCommentedAST(AST_FIT, AstInteger(0x1f0), NULL, $1); }
  | C_FILE C_STRING_LITERAL asm_eoln
    { $$ = NewCommentedAST(AST_FILE, GetFullFileName($2), NULL, $1); }
  | C_DEBUG '(' ')' asm_eoln
    {
        AST *ast = NULL;
        AST *comment = $1;
        if (comment) {
            ast = NewAST(AST_COMMENTEDNODE, ast, comment);
        }
        $$ = ast;
    }
  | C_DEBUG '(' asm_debug_exprlist ')' asm_eoln
    {
        AST *ast = BuildDebugList($3);
        AST *comment = $1;
        if (comment) {
            ast = NewAST(AST_COMMENTEDNODE, ast, comment);
        }
        $$ = ast;
    }
  ;

asm_debug_exprlist:
   asm_debug_expritem_first
     { $$ = $1; }
   | asm_debug_expritem_first ',' asm_debug_exprlist_continue
     { $$ = AddToList($1, $3); }
;

asm_debug_expritem_first:
  asm_debug_expritem
    {
        AST *list = $1;
        AST *note = NewAST(AST_LABEL, NULL, NULL);

        $$ = NewAST(AST_EXPRLIST, note, list);
    }
;
   
asm_debug_exprlist_continue:
   asm_debug_expritem
     { $$ = $1; }
   | asm_debug_exprlist_continue ',' asm_debug_expritem
     { $$ = AddToList($1, $3); }
;

asm_debug_expritem: asm_operand
;

asm_operand:
  asmexpr
   { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
 | '#' asmexpr
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_IMMHOLDER, $2, NULL), NULL); }
 | '=' asmexpr
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_IMMHOLDER, $2, NULL), NULL); }
 | '#' '#' asmexpr
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_BIGIMMHOLDER, $3, NULL), NULL); }
 | C_EQ_OP asmexpr
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_BIGIMMHOLDER, $2, NULL), NULL); }
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
	| C_THROWIF expression ';'
          {
              AST *top = NewCommentedAST(AST_THROW, $2, NULL, $1);
              AST *throwit;
              if (top && top->kind != AST_THROW) {
                  throwit = top->left;
              } else {
                  throwit = top;
              }
              throwit->d.ival = 1;
              $$ = top;
          }
        ;

try_statement
	: C_TRY compound_statement handler_sequence
          {
              AST *tryblock = $2;
              AST *catchlist = $3;
              AST *catchseq;
              AST *catchvar, *catchblock, *catchdecl;
              AST *try_if = NULL;
              AST *trytest;

              while (catchlist) {
                  catchseq = catchlist->left;
                  catchlist = catchlist->right;
                  catchdecl = catchseq->left;
                  catchblock = catchseq->right;

                  catchvar = catchdecl->left->right;
                  catchblock = NewAST(AST_STMTLIST, AstAssign(catchvar, NewAST(AST_CATCHRESULT, NULL, NULL)), catchblock);
                  catchblock = NewAST(AST_STMTLIST, catchdecl, catchblock);

                  trytest = NewAST(AST_SETJMP, NULL, NULL);
                  try_if = NewAST(AST_IF,
                                  AstOperator(K_EQ, trytest, AstInteger(0)),
                                  NewAST(AST_THENELSE, tryblock, catchblock));
                  try_if = NewCommentedStatement(try_if);
                  tryblock = NewAST(AST_TRYENV, try_if, NULL);
              }
              $$ = tryblock;
          }
        ;

handler_sequence
	: handler_item
          { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
	| handler_sequence handler_item
          {
              AST *last = NewAST(AST_LISTHOLDER, $2, NULL);
              $$ = AddToList($1, last);
          }
        ;

handler_item
	: C_CATCH push_current_types '(' handler_declarator ')' compound_statement pop_current_types
          {
              AST *declarator = $4;
              AST *block = $6;
              // NOTE: the declarator is wrapped in a stmtlist
              $$ = NewAST(AST_SEQUENCE, declarator, block);
          }
	;

push_current_types
	: { PushCurrentTypes(); }
	;
pop_current_types
	: { PopCurrentTypes(); }
	;

handler_declarator
	: raw_parameter_declaration
          {
              AST *decl = MakeDeclarations($1, currentTypes);
              $$ = decl;
          }
        | C_ELLIPSIS
          {
              AST *var = AstTempIdentifier("catch");
              var = NewAST(AST_DECLARE_VAR, ast_type_generic, var);
              $$ = var;
          }
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
                AST *ident = NULL;
                Module *module = NULL;
                int is_public = 1;

                decl = MergeOldStyleDeclarationList(decl, decl_list);
                type = CombineTypes(type, decl, &ident, &module);
                if (module) {
                    ERROR(decl, ":: not supported yet for old style functions");
                }
                DeclareCTypedFunction(current, type, ident, is_public, body, NULL);
            }
	| declaration_specifiers declarator attribute_decl compound_statement_or_fromfile
            {
                AST *type = $1;
                AST *decl = $2;
                AST *anno = $3;
                AST *body = $4;
                AST *ident = NULL;
                Module *module = NULL;
                int is_public = 1;
                type = CombineTypes(type, decl, &ident, &module);
                if (module) {
                    DeclareCTypedFunction(module, type, ident, is_public, body, anno);
                } else {
                    DeclareCTypedFunction(current, type, ident, is_public, body, anno);
                }
            }
	| declarator func_declaration_list compound_statement_or_fromfile
            {
                AST *type;
                AST *ident = NULL;
                Module *module = NULL;
                AST *decl = $1;
                AST *decl_list = $2;
                AST *body = $3;
                int is_public = 1;
                decl = MergeOldStyleDeclarationList(decl, decl_list);
                type = CombineTypes(NULL, decl, &ident, &module);
                if (module) {
                    ERROR(decl, ":: not allowed with old style function declarations");
                }
                DeclareCTypedFunction(current, type, ident, is_public, body, NULL);
            }
	| declarator attribute_decl compound_statement_or_fromfile
            {
                AST *type;
                AST *ident = NULL;
                Module *module = NULL;
                AST *decl = $1;
                AST *anno = $2;
                AST *body = $3;
                int is_public = 1;
                type = CombineTypes(NULL, decl, &ident, &module);
                if (module) {
                    ERROR(decl, ":: needs fixing");
                }
                DeclareCTypedFunction(current, type, ident, is_public, body, anno);
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

compound_statement_or_fromfile
        : fromfile_decl
        | compound_statement
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
  asm_eoln
    { $$ = NULL; }
  | error asm_eoln
    { $$ = NULL; }
  | C_BYTE asm_eoln
    { $$ = NewCommentedAST(AST_BYTELIST, NULL, NULL, $1); }
  | C_BYTE pasm_operandlist asm_eoln
    { $$ = NewCommentedAST(AST_BYTELIST, $2, NULL, $1); }
  | C_WORD asm_eoln
    { $$ = NewCommentedAST(AST_WORDLIST, NULL, NULL, $1); }
  | C_WORD pasm_operandlist asm_eoln
    { $$ = NewCommentedAST(AST_WORDLIST, $2, NULL, $1); }
  | C_LONG asm_eoln
    { $$ = NewCommentedAST(AST_LONGLIST, NULL, NULL, $1); }
  | C_LONG pasm_operandlist asm_eoln
    { $$ = NewCommentedAST(AST_LONGLIST, $2, NULL, $1); }
  | instruction asm_eoln
    { $$ = NewCommentedInstr($1); }
  | instruction pasm_operandlist asm_eoln
    { $$ = NewCommentedInstr(AddToList($1, $2)); }
  | instruction modifierlist asm_eoln
    { $$ = NewCommentedInstr(AddToList($1, $2)); }
  | instruction pasm_operandlist modifierlist asm_eoln
    { $$ = NewCommentedInstr(AddToList($1, AddToList($2, $3))); }
  | C_ALIGNL asm_eoln
    { $$ = NewCommentedAST(AST_ALIGN, AstInteger(4), NULL, $1); }
  | C_ALIGNW asm_eoln
    { $$ = NewCommentedAST(AST_ALIGN, AstInteger(2), NULL, $1); }
  | C_ORG asm_eoln
    { $$ = NewCommentedAST(AST_ORG, NULL, NULL, $1); }
  | C_ORG pasmexpr asm_eoln
    { $$ = NewCommentedAST(AST_ORG, $2, NULL, $1); }
  | C_ORGH asm_eoln
    { $$ = NewCommentedAST(AST_ORGH, NULL, NULL, $1); }
  | C_ORGH pasmexpr asm_eoln
    { $$ = NewCommentedAST(AST_ORGH, $2, NULL, $1); }
  | C_ORGF pasmexpr asm_eoln
    { $$ = NewCommentedAST(AST_ORGF, $2, NULL, $1); }
  | C_RES pasmexpr asm_eoln
    { $$ = NewCommentedAST(AST_RES, $2, NULL, $1); }
  | C_FIT pasmexpr asm_eoln
    { $$ = NewCommentedAST(AST_FIT, $2, NULL, $1); }
  | C_FIT asm_eoln
    { $$ = NewCommentedAST(AST_FIT, AstInteger(0x1f0), NULL, $1); }
  | C_FILE C_STRING_LITERAL asm_eoln
    { $$ = NewCommentedAST(AST_FILE, GetFullFileName($2), NULL, $1); }
  | C_DEBUG '(' ')' asm_eoln
    {
        AST *ast = NULL;
        AST *comment = $1;
        if (comment) {
            ast = NewAST(AST_COMMENTEDNODE, ast, comment);
        }
        $$ = ast;
    }
  | C_DEBUG '(' pasm_debug_exprlist ')' asm_eoln
    {
        AST *ast = BuildDebugList($3);
        AST *comment = $1;
        if (comment) {
            ast = NewAST(AST_COMMENTEDNODE, ast, comment);
        }
        $$ = ast;
    }
  ;

pasm_debug_exprlist:
   pasm_debug_expritem_first
     { $$ = $1; }
   | pasm_debug_expritem_first ',' pasm_debug_exprlist_continue
     { $$ = AddToList($1, $3); }
;

pasm_debug_expritem_first:
  pasm_debug_expritem
    {
        AST *list = $1;
        AST *note = NewAST(AST_LABEL, NULL, NULL);

        $$ = NewAST(AST_EXPRLIST, note, list);
    }
;
   
pasm_debug_exprlist_continue:
   pasm_debug_expritem
     { $$ = $1; }
   | pasm_debug_exprlist_continue ',' pasm_debug_expritem
     { $$ = AddToList($1, $3); }
;

pasm_debug_expritem: pasm_operand
;

pasm_operand:
  pasmexpr
   { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
 | '#' pasmexpr
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_IMMHOLDER, $2, NULL), NULL); }
 | '#' '#' pasmexpr
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_BIGIMMHOLDER, $3, NULL), NULL); }
/* | pasmexpr '[' pasmexpr ']'
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_ARRAYREF, $1, $3), NULL); } */
;

pasm_operandlist:
   pasm_operand
   { $$ = $1; }
 | pasm_operandlist ',' pasm_operand
   { $$ = AddToList($1, $3); }
 ;

optpasmrange
      : '[' pasmexpr ']'
            { $$ = $2; }
      | '[' '#' '#' pasmexpr ']'
            { $$ = NewAST(AST_BIGIMMHOLDER, $4, NULL); }
      | '[' C_EQ_OP pasmexpr ']'
            { $$ = NewAST(AST_BIGIMMHOLDER, $3, NULL); }
      | /* nothing */
            { $$ = NULL; }
;

pasmatom
      : C_IDENTIFIER
            { $$ = $1; }
      | C_CONSTANT
            { $$ = $1; }
      | '$'
            { $$ = NewAST(AST_HERE, NULL, NULL); }
      | C_HWREG optpasmrange
            {
                AST *reg = $1;
                AST *index = $2;
                if (index) {
                    if (index->kind == AST_BIGIMMHOLDER) {
                        index->left = NewAST(AST_RANGEREF, reg, index->left);
                        $$ = index;
                    } else {
                        $$ = NewAST(AST_RANGEREF, reg, index);
                    }
                } else {
                    $$ = reg;
                }
            }
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
      | pasmatom '(' pasm_operandlist ')'
            { $$ = NewAST(AST_FUNCCALL, $1, $3); }
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
    
    SETCOLOR(PRINT_ERROR);
    ERRORHEADER(current->Lptr->fileName, current->Lptr->lineCounter, "error");

    if (!strcmp(msg, "syntax error, unexpected $end")) {
        fprintf(stderr, "unexpected end of input");
        msg = "";
    }
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
    RESETCOLOR();
}

