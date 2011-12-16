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

/* other stuff */
%token T_INDENT
%token T_OUTDENT
%token T_EOF

%%
input:
  T_EOF /* empty */
  | topelement input
  ;

topelement:
  T_CON conblock
;

conblock:
  condecl
  | condecl conblock
  ;

condecl:
  T_IDENTIFIER '=' T_NUM
  ;
%%

void
yyerror(char *msg)
{
    fprintf(stderr, "error %s\n", msg);
}
