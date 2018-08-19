%token C_IDENTIFIER C_CONSTANT C_STRING_LITERAL C_SIZEOF
%token C_PTR_OP C_INC_OP C_DEC_OP C_LEFT_OP C_RIGHT_OP C_LE_OP C_GE_OP C_EQ_OP C_NE_OP
%token C_AND_OP C_OR_OP C_MUL_ASSIGN C_DIV_ASSIGN C_MOD_ASSIGN C_ADD_ASSIGN
%token C_SUB_ASSIGN C_LEFT_ASSIGN C_RIGHT_ASSIGN C_AND_ASSIGN
%token C_XOR_ASSIGN C_OR_ASSIGN C_TYPE_NAME

%token C_TYPEDEF C_EXTERN C_STATIC C_AUTO C_REGISTER
%token C_CHAR C_SHORT C_INT C_LONG C_SIGNED C_UNSIGNED C_FLOAT C_DOUBLE C_CONST C_VOLATILE C_VOID
%token C_STRUCT C_UNION C_ENUM C_ELLIPSIS

%token C_CASE C_DEFAULT C_IF C_ELSE C_SWITCH C_WHILE C_DO C_FOR C_GOTO C_CONTINUE C_BREAK C_RETURN

%start translation_unit
%%

primary_expression
	: C_IDENTIFIER
	| C_CONSTANT
	| C_STRING_LITERAL
	| '(' expression ')'
	;

postfix_expression
	: primary_expression
	| postfix_expression '[' expression ']'
	| postfix_expression '(' ')'
	| postfix_expression '(' argument_expression_list ')'
	| postfix_expression '.' C_IDENTIFIER
	| postfix_expression C_PTR_OP C_IDENTIFIER
	| postfix_expression C_INC_OP
	| postfix_expression C_DEC_OP
	;

argument_expression_list
	: assignment_expression
	| argument_expression_list ',' assignment_expression
	;

unary_expression
	: postfix_expression
	| C_INC_OP unary_expression
	| C_DEC_OP unary_expression
	| unary_operator cast_expression
	| C_SIZEOF unary_expression
	| C_SIZEOF '(' type_name ')'
	;

unary_operator
	: '&'
	| '*'
	| '+'
	| '-'
	| '~'
	| '!'
	;

cast_expression
	: unary_expression
	| '(' type_name ')' cast_expression
	;

multiplicative_expression
	: cast_expression
	| multiplicative_expression '*' cast_expression
	| multiplicative_expression '/' cast_expression
	| multiplicative_expression '%' cast_expression
	;

additive_expression
	: multiplicative_expression
	| additive_expression '+' multiplicative_expression
	| additive_expression '-' multiplicative_expression
	;

shift_expression
	: additive_expression
	| shift_expression C_LEFT_OP additive_expression
	| shift_expression C_RIGHT_OP additive_expression
	;

relational_expression
	: shift_expression
	| relational_expression '<' shift_expression
	| relational_expression '>' shift_expression
	| relational_expression C_LE_OP shift_expression
	| relational_expression C_GE_OP shift_expression
	;

equality_expression
	: relational_expression
	| equality_expression C_EQ_OP relational_expression
	| equality_expression C_NE_OP relational_expression
	;

and_expression
	: equality_expression
	| and_expression '&' equality_expression
	;

exclusive_or_expression
	: and_expression
	| exclusive_or_expression '^' and_expression
	;

inclusive_or_expression
	: exclusive_or_expression
	| inclusive_or_expression '|' exclusive_or_expression
	;

logical_and_expression
	: inclusive_or_expression
	| logical_and_expression C_AND_OP inclusive_or_expression
	;

logical_or_expression
	: logical_and_expression
	| logical_or_expression C_OR_OP logical_and_expression
	;

conditional_expression
	: logical_or_expression
	| logical_or_expression '?' expression ':' conditional_expression
	;

assignment_expression
	: conditional_expression
	| unary_expression assignment_operator assignment_expression
	;

assignment_operator
	: '='
	| C_MUL_ASSIGN
	| C_DIV_ASSIGN
	| C_MOD_ASSIGN
	| C_ADD_ASSIGN
	| C_SUB_ASSIGN
	| C_LEFT_ASSIGN
	| C_RIGHT_ASSIGN
	| C_AND_ASSIGN
	| C_XOR_ASSIGN
	| C_OR_ASSIGN
	;

expression
	: assignment_expression
	| expression ',' assignment_expression
	;

constant_expression
	: conditional_expression
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
