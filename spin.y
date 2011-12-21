/*
 * Spin compiler parser
 * Copyright (c) 2011 Total Spectrum Software Inc.
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include "spinc.h"

/* Yacc functions */
    void yyerror(char *);
    int yylex(void);
%}

%token T_IDENTIFIER
%token T_NUM

/* various keywords */
%token T_CON
%token T_VAR
%token T_DAT
%token T_PUB
%token T_PRI
%token T_OBJ

%token T_BYTE
%token T_WORD
%token T_LONG

%token T_REPEAT
%token T_IF
%token T_IFNOT
%token T_ELSE
%token T_ELSEIF
%token T_ELSEIFNOT

/* operators */
%token T_GE
%token T_LE
%token T_NE
%token T_EQ
%token T_MODULUS
%token T_HIGHMULT

/* assignment operators */
%token T_ASSIGN

/* other stuff */
%token T_RETURN
%token T_INDENT
%token T_OUTDENT
%token T_EOLN
%token T_EOF

/* precedence of operators */
%left '<' '>' T_GE T_LE T_NE T_EQ
%left '-' '+'
%left '*' '/'

%%
input:
  topelement
  | topelement input
  ;

topelement:
  T_EOLN
  | T_CON T_EOLN conblock
  { $$ = current->conblock = AddToList(current->conblock, $3); }
  | T_DAT T_EOLN datblock
  { $$ = current->datblock = AddToList(current->datblock, $3); }
  | T_VAR T_EOLN varblock
  { $$ = current->varblock = AddToList(current->varblock, $3); }
  | T_PUB funcdef funcbody
  | T_PRI funcdef funcbody
;

funcdef:
  T_IDENTIFIER optparamlist localvars T_EOLN
;

optparamlist:
/* nothing */
| paramlist
;

paramlist:
  identifier
| paramlist ',' identifier
  { $$ = AddToList($1, $3); }
  ;

localvars:
/* nothing */
| '|' identlist
  { $$ = $2 }
    ;

funcbody:
  stmt
| funcbody stmt
  ;

stmt:
  T_RETURN expr
  ;

conblock:
  conline
  { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
  | conblock conline
  { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $2, NULL)); }
  ;

conline:
  identifier '=' expr T_EOLN
     { $$ = NewAST(AST_ASSIGN, $1, $3); }
  | enumlist T_EOLN
     { $$ = $1; }
  ;

enumlist:
  enumitem
  | enumlist ',' enumitem
    { $$ = AddToList($1, $3); }
  ;

enumitem:
  identifier
  { $$ = $1; }
  | '#' expr
  { $$ = NewAST(AST_ENUMSET, $2, NULL); }
  ;

datblock:
  datline
  | datblock datline
  ;

datline:
  optsymbol sizespec datalist T_EOLN
  ;

datalist:
  dataelem
  | datalist ',' dataelem
  ;

dataelem:
  optsize expr optcount
;

varblock:
    varline
    { $$ = $1; }
  | varblock varline
    { $$ = AddToList($1, $2); }
  ;

varline:
  T_BYTE identlist T_EOLN
    { $$ = NewAST(AST_BYTELIST, $2, NULL); }
  | T_WORD identlist T_EOLN
    { $$ = NewAST(AST_WORDLIST, $2, NULL); }
  | T_LONG identlist T_EOLN
    { $$ = NewAST(AST_LONGLIST, $2, NULL); }
  ;

identlist:
  identdecl
  { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
  | identlist ',' identdecl
  { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $3, NULL)); }
  ;

identdecl:
  identifier
  { $$ = $1; }
  | identifier '[' expr ']'
  { $$ = NewAST(AST_ARRAYDECL, $1, $3); }
  ;

optsize:
  | sizespec
  ;

optcount:
  | '[' expr ']'
  ;

optsymbol:
  | identifier
  ;

expr:
  integer
  | lhs
  | expr '+' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '+'; }
  | expr '-' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '-'; }
  | expr '*' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '*'; }
  | expr '/' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '/'; }
  | expr '>' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '>'; }
  | expr '<' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '<'; }
  | expr T_GE expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = T_GE; }
  | expr T_LE expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = T_LE; }
  | expr T_NE expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = T_NE; }
  | expr T_EQ expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = T_EQ; }
  | expr T_MODULUS expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = T_MODULUS; }
  | expr T_HIGHMULT expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = T_HIGHMULT; }
  | lhs T_ASSIGN expr
    { $$ = NewAST(AST_ASSIGN, $1, $3); $$->d.ival = T_ASSIGN; }
  | '(' expr ')'
  ;

lhs: identifier
  | lhs '[' expr ']'
    { $$ = NewAST(AST_ARRAYDECL, $1, $3); }
  ;

sizespec:
  T_BYTE
  | T_WORD
  | T_LONG
  ;

integer:
  T_NUM
  { $$ = current->ast; }
;

identifier:
  T_IDENTIFIER
  { $$ = current->ast; }
;
 
%%

void
yyerror(char *msg)
{
    fprintf(stderr, "error %s\n", msg);
}
