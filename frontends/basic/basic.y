/*
 * Spin compiler parser
 * Copyright (c) 2011-2018 Total Spectrum Software Inc.
 * See the file COPYING for terms of use.
 */

%define api.prefix {basicyy}

%{
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "basic.h"

/* Yacc functions */
    void basicyyerror(const char *);
    int basicyylex();

    extern int gl_errors;

    extern AST *last_ast;

#define YYERROR_VERBOSE 1
%}

%pure-parser

%token BAS_IDENTIFIER "identifier"
%token BAS_INTEGER    "integer"
%token BAS_FLOAT      "number"
%token BAS_STRING     "string"

/* keywords */
%token BAS_AS         "as"
%token BAS_ASM        "asm"
%token BAS_CONTINUE   "continue"
%token BAS_DIM        "dim"
%token BAS_DO         "do"
%token BAS_ELSE       "else"
%token BAS_END        "end"
%token BAS_EXIT       "exit"
%token BAS_FOR        "for"
%token BAS_FUNCTION   "function"
%token BAS_IF         "if"
%token BAS_LET        "let"
%token BAS_LOOP       "loop"
%token BAS_MOD        "mod"
%token BAS_NEXT       "next"
%token BAS_RETURN     "return"
%token BAS_STEP       "step"
%token BAS_SUB        "sub"
%token BAS_TO         "to"
%token BAS_UNTIL      "until"
%token BAS_WHILE      "while"

%%
statement_list:
  statement
  | statement statement_list
;

statement:
  BAS_IDENTIFIER "=" expr
  ;

expr:
  BAS_NUMBER
  ;

%%
void
basicyyerror(const char *msg)
{
    extern int saved_basicyychar;
    int yychar = saved_basicyychar;
    
    fprintf(stderr, "%s:%d: error: ", current->L.fileName, current->L.lineCounter);

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
