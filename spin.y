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

%union {
    unsigned long ival; /* integer value */
    char *string;      /* identifier string */
}

%token <string> T_IDENTIFIER
%token <ival>   T_NUM

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

/* other stuff */
%token T_INDENT
%token T_OUTDENT
%token T_EOLN
%token T_EOF

%%
input:
  /* empty */
  | topelement input
  ;

topelement:
  T_CON T_EOLN conblock
  | T_DAT T_EOLN datblock
;

conblock:
  conline
  | conblock conline
  ;

conline:
  T_IDENTIFIER '=' T_NUM T_EOLN
  | enumlist T_EOLN
  ;

enumlist:
  | enumitem
  | enumlist ',' enumitem
  ;

enumitem:
  T_IDENTIFIER
  | '#' T_NUM
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

optsize:
  | sizespec
  ;

optcount:
  | '[' expr ']'
  ;

optsymbol:
  | T_IDENTIFIER
  ;

expr:
  T_NUM
  | T_IDENTIFIER
  | '(' expr ')'
  ;

sizespec:
  T_BYTE
  | T_WORD
  | T_LONG
  ;

%%

void
yyerror(char *msg)
{
    fprintf(stderr, "error %s\n", msg);
}
