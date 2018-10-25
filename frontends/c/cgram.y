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
    
#define YYERROR_VERBOSE 1
#define YYSTYPE AST*

static AST *
C_ModifySignedUnsigned(AST *modifier, AST *type)
{
    return type;
}

static AST *
CombineTypes(AST *first, AST *second, AST **identifier)
{
    if (!second) {
        return first;
    }
    if (second->kind == AST_DECLARE_VAR) {
        first = CombineTypes(first, second->left, identifier);
        first = CombineTypes(first, second->right, identifier);
        return first;
    }
    if (second->kind == AST_IDENTIFIER) {
        *identifier = second;
        return first;
    }
    if (second->kind == AST_ARRAYDECL) {
        first = NewAST(AST_ARRAYTYPE, first, second->right);
        return CombineTypes(first, second->left, identifier);
    }
    if (second->kind == AST_FUNCTYPE) {
        second->left = CombineTypes(first, second->left, identifier);
        return second;
    }
    if (second->kind == AST_PTRTYPE) {
        second->left = CombineTypes(first, second->left, identifier);
        return second;
    }
    if (second->kind == AST_ASSIGN) {
        AST *expr, *ident;
        expr = second->right;
        first = CombineTypes(first, second->left, &ident);
        ident = AstAssign(ident, expr);
        *identifier = ident;
        return first;
    }
    ERROR(first, "Internal error: don't know how to combine types");
    return first;
}

AST *
MergeDeclareVar(AST *decl_spec, AST *declarator)
{
    AST *type, *ident;

    ident = NULL;
    type = CombineTypes(decl_spec, declarator, &ident);
    return NewAST(AST_DECLARE_VAR, type, ident);
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

%token C_EOF "end of file"

%start translation_unit
%%

primary_expression
	: C_IDENTIFIER
            { $$ = $1; }
	| C_CONSTANT
            { $$ = $1; }
	| C_STRING_LITERAL
            { $$ = $1; }
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
	;

argument_expression_list
	: assignment_expression
            { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
	| argument_expression_list ',' assignment_expression
            { $$ = AddToList($1, NewAST(AST_EXPRLIST, $1, NULL)); }
	;

unary_expression
	: postfix_expression
            { $$ = $1; }
	| C_INC_OP unary_expression
            { $$ = AstOperator(K_INCREMENT, NULL, $1); }
	| C_DEC_OP unary_expression
            { $$ = AstOperator(K_DECREMENT, NULL, $1); }
	| unary_operator cast_expression
            { $$ = $1; $$->left = $2; }
	| C_SIZEOF unary_expression
            { $$ = AstOperator(AST_SIZEOF, $1, NULL); }           
	| C_SIZEOF '(' type_name ')'
            { $$ = AstOperator(AST_SIZEOF, $1, NULL); }           
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
            { $$ = AstOperator(K_SHR, $1, $3); }
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
            { $$ = AstOpAssign(K_SHR, NULL, NULL); }
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
            { $$ = MergeDeclareVar(NULL, $1); }
	| declaration_specifiers init_declarator_list ';'
            {
                $$ = MergeDeclareVar($1, $2);
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
	| init_declarator_list ',' init_declarator
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
	| type_specifier
	| type_qualifier specifier_qualifier_list
	| type_qualifier
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
            {
                $$ = $1;
                $$->left = AddToList($$->left, $2);
            }
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
            { $$ = NewAST(AST_DECLARE_VAR, NewAST(AST_FUNCTYPE, NULL, $3), $1); }
	| direct_declarator '(' identifier_list ')'
            { $$ = NewAST(AST_DECLARE_VAR, NewAST(AST_FUNCTYPE, NULL, $3), $1); }
	| direct_declarator '(' ')'
            { $$ = NewAST(AST_DECLARE_VAR, NewAST(AST_FUNCTYPE, NULL, NULL), $1); }
	;

pointer
	: '*'
            { $$ = NewAST(AST_PTRTYPE, NULL, NULL); }
	| '*' type_qualifier_list
            { $$ = NewAST(AST_PTRTYPE, $2, NULL); }
	| '*' pointer
            { $$ = NewAST(AST_PTRTYPE, $2, NULL); }
	| '*' type_qualifier_list pointer
            {
                $$ = NewAST(AST_PTRTYPE, $2, NULL);
                $$ = AddToList($$, $3);
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
           { $$ = $1; }
	;

parameter_list
	: parameter_declaration
            { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
	| parameter_list ',' parameter_declaration
            { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $3, NULL)); }
	;

parameter_declaration
	: declaration_specifiers declarator
            { $$ = MergeDeclareVar($1, $2); }
	| declaration_specifiers abstract_declarator
            { $$ = MergeDeclareVar($1, $2); }
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
	| specifier_qualifier_list abstract_declarator
	;

abstract_declarator
	: pointer
	| direct_abstract_declarator
	| pointer direct_abstract_declarator
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
	| '{' initializer_list '}'
	| '{' initializer_list ',' '}'
	;

initializer_list
	: initializer
	| initializer_list ',' initializer
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
	| C_CASE constant_expression ':' statement
	| C_DEFAULT ':' statement
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
                          NewAST(AST_THENELSE, $5, NULL)); }
	| C_IF '(' expression ')' statement C_ELSE statement
            { $$ = NewAST(AST_IF, $3,
                          NewAST(AST_THENELSE, $5, $7)); }
	| C_SWITCH '(' expression ')' statement
            { $$ = NULL; }
	;

iteration_statement
	: C_WHILE '(' expression ')' statement
            { AST *body = CheckYield($5);
              $$ = NewCommentedAST(AST_WHILE, $3, body, $1);
            }
	| C_DO statement C_WHILE '(' expression ')' ';'
            { AST *body = CheckYield($2);
              $$ = NewCommentedAST(AST_DOWHILE, $5, body, $1);
            }
	| C_FOR '(' expression_statement expression_statement ')' statement
            {   AST *body = CheckYield($6);
                AST *init = $3;
                AST *update = $4;
                AST *cond = AstInteger(1);
                AST *stepstmt = NewAST(AST_STEP, update, body);
                AST *condtest = NewAST(AST_TO, cond, stepstmt);
                $$ = NewCommentedAST(AST_FOR, init, condtest, $1);
            }
	| C_FOR '(' expression_statement expression_statement expression ')' statement
            {   AST *body = CheckYield($7);
                AST *init = $3;
                AST *update = $4;
                AST *cond = $5;
                AST *stepstmt = NewAST(AST_STEP, update, body);
                AST *condtest = NewAST(AST_TO, cond, stepstmt);
                $$ = NewCommentedAST(AST_FOR, init, condtest, $1);
            }
	;

jump_statement
	: C_GOTO C_IDENTIFIER ';'
	| C_CONTINUE ';'
	| C_BREAK ';'
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
