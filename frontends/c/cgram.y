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
    void basicyyerror(const char *);
    int basicyylex();

    extern int gl_errors;

    extern AST *last_ast;
    extern AST *CommentedListHolder(AST *); // in spin.y
    
#define YYERROR_VERBOSE 1
#define YYSTYPE AST*

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
	| C_SIZEOF '(' type_name ')'
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
            { $$ = AstOperator(K_MODULO, $1, $3); }
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
            { $$ = AstOpAssign(K_MOD, NULL, NULL); }
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
	| declaration_specifiers init_declarator_list ';'
	;

declaration_specifiers
	: storage_class_specifier
	| storage_class_specifier declaration_specifiers
	| type_specifier
	| type_specifier declaration_specifiers
	| type_qualifier
	| type_qualifier declaration_specifiers
	;

init_declarator_list
	: init_declarator
	| init_declarator_list ',' init_declarator
	;

init_declarator
	: declarator
	| declarator '=' initializer
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
	| C_CHAR
	| C_SHORT
	| C_INT
	| C_LONG
	| C_FLOAT
	| C_DOUBLE
	| C_SIGNED
	| C_UNSIGNED
	| struct_or_union_specifier
	| enum_specifier
	| C_TYPE_NAME
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
	| C_VOLATILE
	;

declarator
	: pointer direct_declarator
	| direct_declarator
	;

direct_declarator
	: C_IDENTIFIER
	| '(' declarator ')'
	| direct_declarator '[' constant_expression ']'
	| direct_declarator '[' ']'
	| direct_declarator '(' parameter_type_list ')'
	| direct_declarator '(' identifier_list ')'
	| direct_declarator '(' ')'
	;

pointer
	: '*'
	| '*' type_qualifier_list
	| '*' pointer
	| '*' type_qualifier_list pointer
	;

type_qualifier_list
	: type_qualifier
	| type_qualifier_list type_qualifier
	;


parameter_type_list
	: parameter_list
	| parameter_list ',' C_ELLIPSIS
	;

parameter_list
	: parameter_declaration
	| parameter_list ',' parameter_declaration
	;

parameter_declaration
	: declaration_specifiers declarator
	| declaration_specifiers abstract_declarator
	| declaration_specifiers
	;

identifier_list
	: C_IDENTIFIER
	| identifier_list ',' C_IDENTIFIER
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
	| '{' statement_list '}'
	| '{' declaration_list '}'
	| '{' declaration_list statement_list '}'
	;

declaration_list
	: declaration
	| declaration_list declaration
	;

statement_list
	: statement
	| statement_list statement
	;

expression_statement
	: ';'
	| expression ';'
	;

selection_statement
	: C_IF '(' expression ')' statement
	| C_IF '(' expression ')' statement C_ELSE statement
	| C_SWITCH '(' expression ')' statement
	;

iteration_statement
	: C_WHILE '(' expression ')' statement
	| C_DO statement WHILE '(' expression ')' ';'
	| C_FOR '(' expression_statement expression_statement ')' statement
	| C_FOR '(' expression_statement expression_statement expression ')' statement
	;

jump_statement
	: C_GOTO C_IDENTIFIER ';'
	| C_CONTINUE ';'
	| C_BREAK ';'
	| C_RETURN ';'
	| C_RETURN expression ';'
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
	| declaration_specifiers declarator compound_statement
	| declarator declaration_list compound_statement
	| declarator compound_statement
	;

%%
#include <stdio.h>

extern char cgram_yytext[];
extern int column;

cgram_yyerror(s)
char *s;
{
	fflush(stdout);
	printf("\n%*s\n%*s\n", column, "^", column, s);
}
