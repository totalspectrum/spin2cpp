/*
 * Spin compiler parser
 * Copyright (c) 2011-2018 Total Spectrum Software Inc.
 * See the file COPYING for terms of use.
 */

/* %define api.prefix {basicyy} */

%{
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdlib.h>
#include "frontends/common.h"
#include "frontends/lexer.h"
    
/* Yacc functions */
    void cgramyyerror(const char *);
    int cgramyylex();

    extern int gl_errors;

    extern AST *last_ast;
    extern AST *CommentedListHolder(AST *); // in spin.y

    extern void DeclareBASICGlobalVariables(AST *);
    
#define YYERROR_VERBOSE 1
#define YYSTYPE AST*

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
C_ModifySignedUnsigned(AST *modifier, AST *type)
{
    if (IsUnsignedType(modifier)) {
        type = MakeSigned(type, 0);
    } else {
        type = MakeSigned(type, 1);
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

static AST *
CombineTypes(AST *first, AST *second, AST **identifier)
{
    AST *expr, *ident;

    if (second && second->kind == AST_COMMENT) {
        second = NULL;
    }
    if (first && first->kind == AST_COMMENT) {
        first = NULL;
    }
    if (!second) {
        return first;
    }
    switch (second->kind) {
    case AST_DECLARE_VAR:
        first = CombineTypes(first, second->left, identifier);
        first = CombineTypes(first, second->right, identifier);
        return first;
    case AST_IDENTIFIER:
        if (identifier) {
            *identifier = second;
        }
        return first;
    case AST_ARRAYDECL:
        first = NewAST(AST_ARRAYTYPE, first, second->right);
        return CombineTypes(first, second->left, identifier);
    case AST_FUNCTYPE:
        second->left = CombineTypes(first, second->left, identifier);
        return second;
    case AST_PTRTYPE:
        second->left = CombineTypes(first, second->left, identifier);
        return second;
    case AST_ASSIGN:
        expr = second->right;
        first = CombineTypes(first, second->left, &ident);
        ident = AstAssign(ident, expr);
        if (identifier) {
            *identifier = ident;
        }
        return first;
    case AST_MODIFIER_CONST:
    case AST_MODIFIER_VOLATILE:
        expr = NewAST(second->kind, NULL, NULL);
        second = CombineTypes(first, second->left, identifier);
        expr->left = second;
        return expr;
    default:
        ERROR(first, "Internal error: don't know how to combine types");
        return first;
    }
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
        ident = NewAST(AST_DECLARE_VAR, type, ident);
        stmtlist = AddToList(stmtlist, NewAST(AST_STMTLIST, ident, NULL));
    }
    return stmtlist;
}

AST *
SingleDeclareVar(AST *decl_spec, AST *declarator)
{
    AST *type, *ident;

    ident = NULL;
    type = CombineTypes(decl_spec, declarator, &ident);
    return NewAST(AST_DECLARE_VAR, type, ident);
}

static void
DeclareCGlobalVariables(AST *slist)
{
    while (slist) {
        if (slist->kind != AST_STMTLIST) {
            ERROR(slist, "internal error in DeclareCGlobalVars");
        }
        DeclareBASICGlobalVariables(slist->left);
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
        if (entry->kind == AST_DECLARE_VAR) {
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

%}

%pure-parser

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

%token C_TYPEDEF "typedef"
%token C_EXTERN "extern"
%token C_STATIC "static"
%token C_AUTO "auto"
%token C_REGISTER "register"
%token C_CHAR  "char"
%token C_SHORT "short"
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

%token C_ASM "__asm__"
%token C_INSTR "asm instruction"
%token C_INSTRMODIFIER "instruction modifier"

%token C_BUILTIN_ALLOCA
%token C_BUILTIN_PRINTF

%token C_EOF "end of file"

%start translation_unit
%%

primary_expression
	: C_IDENTIFIER
            { $$ = $1; }
	| C_CONSTANT
            { $$ = $1; }
	| C_STRING_LITERAL
            { $$ = NewAST(AST_STRINGPTR, NewAST(AST_EXPRLIST, $1, NULL), NULL); }
        | C_BUILTIN_PRINTF
            { $$ = NewAST(AST_PRINT, NULL, NULL); }
        | C_BUILTIN_ALLOCA
            { $$ = NewAST(AST_ALLOCA, NULL, NULL); }
	| '(' expression ')'
            { $$ = $2; }
	;

postfix_expression
	: primary_expression
            { $$ = $1; }
	| postfix_expression '[' expression ']'
            { $$ = NewAST(AST_ARRAYREF, $1, $3); }
	| postfix_expression '(' ')'
            { $$ = NewAST(AST_FUNCCALL, $1, NULL); }
	| postfix_expression '(' argument_expression_list ')'
            { $$ = NewAST(AST_FUNCCALL, $1, $3); }
	| postfix_expression '.' C_IDENTIFIER
            { $$ = NewAST(AST_METHODREF, $1, $3); }
	| postfix_expression C_PTR_OP C_IDENTIFIER
            { $$ = NewAST(AST_METHODREF, $1, $3); }
	| postfix_expression C_INC_OP
            { $$ = AstOperator(K_INCREMENT, $1, NULL); }
	| postfix_expression C_DEC_OP
            { $$ = AstOperator(K_DECREMENT, $1, NULL); }
        | '(' type_name ')' '{' initializer_list '}'
            {  SYNTAX_ERROR("inline struct expressions not supported yet"); }
        | '(' type_name ')' '{' initializer_list ',' '}'
            {  SYNTAX_ERROR("inline struct expressions not supported yet"); }
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
                if ($$->right)
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
	| storage_class_specifier declaration_specifiers
	| type_specifier
            { $$ = $1; }
	| type_specifier declaration_specifiers
            { $$ = C_ModifySignedUnsigned($1, $2); }
	| type_qualifier
            { $$ = $1; $$->left = ast_type_long; }
	| type_qualifier declaration_specifiers
           { $$ = $1; $$->left = $2; }
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
	| C_EXTERN
	| C_STATIC
	| C_AUTO
	| C_REGISTER
	;

type_specifier
	: C_VOID
            { $$ = ast_type_void; }
	| C_CHAR
            { $$ = ast_type_byte; }
	| C_SHORT
            { $$ = ast_type_signed_word; }
	| C_INT
            { $$ = ast_type_long; }
	| C_LONG
            { $$ = ast_type_long; }
	| C_FLOAT
            { $$ = ast_type_float; }
	| C_DOUBLE
            { $$ = ast_type_float; }
	| C_SIGNED
            { $$ = ast_type_long; }
	| C_UNSIGNED
            { $$ = ast_type_unsigned_long; }
	| struct_or_union_specifier
            { $$ = $1; }
	| enum_specifier
            { $$ = $1; }
	| C_TYPE_NAME
            { $$ = $1; }
	;

struct_or_union_specifier
	: struct_or_union C_IDENTIFIER '{' struct_declaration_list '}'
	| struct_or_union '{' struct_declaration_list '}'
	| struct_or_union C_IDENTIFIER
	;

struct_or_union
	: C_STRUCT
	| C_UNION
	;

struct_declaration_list
	: struct_declaration
	| struct_declaration_list struct_declaration
	;

struct_declaration
	: specifier_qualifier_list struct_declarator_list ';'
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
	| struct_declarator_list ',' struct_declarator
	;

struct_declarator
	: declarator
	| ':' constant_expression
	| declarator ':' constant_expression
	;

enum_specifier
	: C_ENUM '{' enumerator_list '}'
	| C_ENUM C_IDENTIFIER '{' enumerator_list '}'
	| C_ENUM C_IDENTIFIER
	;

enumerator_list
	: enumerator
	| enumerator_list ',' enumerator
	;

enumerator
	: C_IDENTIFIER
	| C_IDENTIFIER '=' constant_expression
	;

type_qualifier
	: C_CONST
            { $$ = NewAST(AST_MODIFIER_CONST, NULL, NULL); }
	| C_VOLATILE
            { $$ = NewAST(AST_MODIFIER_VOLATILE, NULL, NULL); }
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

parameter_declaration
	: declaration_specifiers declarator
            { $$ = SingleDeclareVar($1, $2); }
	| declaration_specifiers abstract_declarator
            { $$ = SingleDeclareVar($1, $2); }
	| declaration_specifiers
            { $$ = $1; }
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
            { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
	| initializer_list ',' initializer
            { $$ = AddToList($1, NewAST(AST_EXPRLIST, $3, NULL)); }
	;

statement
	: labeled_statement
	| compound_statement
	| expression_statement
	| selection_statement
	| iteration_statement
	| jump_statement
	;

labeled_statement
	: C_IDENTIFIER ':' statement
            {
                AST *label = NewAST(AST_LABEL, $1, NULL);
                $$ = NewAST(AST_STMTLIST, label,
                              NewAST(AST_STMTLIST, $3, NULL));
            }
	| C_CASE constant_expression ':' statement
            {
                SYNTAX_ERROR("case not supported yet");
            }
	| C_DEFAULT ':' statement
            {
                SYNTAX_ERROR("case not supported yet");
            }
	;

compound_statement
	: '{' '}'
            { $$ = NULL; }
	| '{' statement_list '}'
            { $$ = $2; }
	| '{' declaration_list '}'
            { $$ = $2; }
	| '{' declaration_list statement_list '}'
            { $$ = AddToList($2, $3); }
	;

declaration_list
	: declaration
            { $$ = NewAST(AST_STMTLIST, $1, NULL); }
	| declaration_list declaration
            { $$ = AddToList($1, NewAST(AST_STMTLIST, $2, NULL)); }
	;

statement_list
	: statement
            { $$ = NewAST(AST_STMTLIST, $1, NULL); }
	| statement_list statement
            { $$ = AddToList($1, NewAST(AST_STMTLIST, $2, NULL)); }
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
                SYNTAX_ERROR("switch not supported yet");
                $$ = NULL;
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
	| C_FOR '(' expression_statement expression_statement ')' statement
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
            }
	| C_FOR '(' expression_statement expression_statement expression ')' statement
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
            }
	| C_FOR '(' declaration expression_statement ')' statement
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
            }
	| C_FOR '(' declaration expression_statement expression ')' statement
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
            }
	;

jump_statement
	: C_GOTO C_IDENTIFIER ';'
            { $$ = NewCommentedAST(AST_GOTO, $2, NULL, $1); }
	| C_CONTINUE ';'
            { $$ = NewCommentedAST(AST_NEXT, NULL, NULL, $1); }
	| C_BREAK ';'
            { $$ = NewCommentedAST(AST_QUIT, NULL, NULL, $1); }
	| C_RETURN ';'
            { $$ = NewCommentedAST(AST_RETURN, NULL, NULL, $1); }
	| C_RETURN expression ';'
            { $$ = NewCommentedAST(AST_RETURN, $2, NULL, $1); }
	;

translation_unit
	: external_declaration
	| translation_unit external_declaration
	;

external_declaration
	: function_definition
	| declaration
            { DeclareCGlobalVariables($1); }
	;

function_definition
	: declaration_specifiers declarator declaration_list compound_statement
            { SYNTAX_ERROR("old style C function declarations are not allowed"); }
	| declaration_specifiers declarator compound_statement
            {
                AST *type;
                AST *ident;
                AST *body = $3;
                int is_public = 1;
                type = CombineTypes($1, $2, &ident);
                DeclareTypedFunction(current, type, ident, is_public, body);
            }
	| declarator declaration_list compound_statement
            { SYNTAX_ERROR("old style C function declarations are not allowed"); }
	| declarator compound_statement
            {
                AST *type;
                AST *ident;
                AST *body = $2;
                int is_public = 1;
                type = CombineTypes(NULL, $1, &ident);
                DeclareTypedFunction(current, type, ident, is_public, body);
            }
	;

%%
#include <stdio.h>

void
cgramyyerror(const char *msg)
{
    extern int saved_cgramyychar;
    int yychar = saved_cgramyychar;
    
    ERRORHEADER(current->L.fileName, current->L.lineCounter, "error");

    // massage bison's error messages to make them easier to understand
    while (*msg) {
        // say which identifier was unexpected
        if (!strncmp(msg, "unexpected identifier", strlen("unexpected identifier")) && last_ast && last_ast->kind == AST_IDENTIFIER) {
            fprintf(stderr, "unexpected identifier `%s'", last_ast->d.string);
            msg += strlen("unexpected identifier");
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
