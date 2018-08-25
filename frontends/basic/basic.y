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
#include <stdlib.h>
#include "frontends/common.h"
  
/* Yacc functions */
    void basicyyerror(const char *);
    int basicyylex();

    extern int gl_errors;

    extern AST *last_ast;

#define YYERROR_VERBOSE 1
#define BASICYYSTYPE AST*
%}

%pure-parser

%token BAS_IDENTIFIER "identifier"
%token BAS_INTEGER    "integer"
%token BAS_FLOAT      "number"
%token BAS_STRING     "string"
%token BAS_EOLN       "end of line"
%token BAS_EOF        "end of file"

/* keywords */
%token BAS_AS         "as"
%token BAS_ASM        "asm"
%token BAS_CONTINUE   "continue"
%token BAS_DECLARE    "declare"
%token BAS_DIM        "dim"
%token BAS_DO         "do"
%token BAS_ELSE       "else"
%token BAS_END        "end"
%token BAS_EXIT       "exit"
%token BAS_FOR        "for"
%token BAS_FUNCTION   "function"
%token BAS_IF         "if"
%token BAS_LET        "let"
%token BAS_LOCAL      "local"
%token BAS_LOOP       "loop"
%token BAS_MOD        "mod"
%token BAS_NEXT       "next"
%token BAS_PROGRAM    "program"
%token BAS_RETURN     "return"
%token BAS_STEP       "step"
%token BAS_SUB        "sub"
%token BAS_THEN       "then"
%token BAS_TO         "to"
%token BAS_UNTIL      "until"
%token BAS_WHILE      "while"

%token BAS_LE         "<="
%token BAS_GE         ">="
%token BAS_NE         "<>"

%left '<' '>' BAS_LE BAS_GE BAS_NE '='
%left '-' '+'
%left '*' '/' BAS_MOD

%%
toplist:
 /* empty */
 | toplist1
;

toplist1:
 topstatement
 | toplist1 topstatement
;

newlines:
  BAS_EOLN
  | newlines BAS_EOLN
  ;

topstatement:
  statement newlines
  | topdecl newlines
;

statement:
  lhs '=' expr newlines
    { $$ = AstAssign($1, $3); }
  | ifstmt
;

ifstmt:
  BAS_IF expr BAS_THEN newlines elseblock
    { $$ = NewCommentedAST(AST_IF, $2, $5, $1); }
;
elseblock:
  statementlist
    { $$ = NewAST(AST_THENELSE, $1, NULL); }
  | statementlist BAS_ELSE newlines statementlist BAS_END BAS_IF
    { $$ = NewAST(AST_THENELSE, $1, $4); }
;

statementlist:
  statement
    { $$ = NewCommentedStatement($1); }
  | statementlist statement
    { $$ = AddToList($1, $2); }
  ;

identifierlist:
  /* empty */
    { $$ = NULL; }
  | identifierlist1
    { $$ = $1; }
;
identifierlist1:
  identifier
    { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
  | identifierlist1 ',' identifier
  { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $3, NULL)); }
  ;

expr:
  BAS_INTEGER
    { $$ = $1; }
  | BAS_FLOAT
    { $$ = $1; }
  | BAS_STRING
    { $$ = $1; }
  | lhs
    { $$ = $1; }
  | expr '+' expr
    { $$ = AstOperator('+', $1, $3); }
  | expr '-' expr
    { $$ = AstOperator('-', $1, $3); }
  | expr '*' expr
    { $$ = AstOperator('*', $1, $3); }
  | expr '/' expr
    { $$ = AstOperator('/', $1, $3); }
  | expr BAS_MOD expr
    { $$ = AstOperator(K_MODULUS, $1, $3); }
  | '(' expr ')'
    { $$ = $2; }
;

lhs: identifier
    { $$ = $1; }
  | identifier '(' expr ')'
    { $$ = NewAST(AST_ARRAYREF, $1, $3); }
;

identifier:
  BAS_IDENTIFIER
    { $$ = $1; }
;

topdecl:
  BAS_SUB BAS_IDENTIFIER '(' identifierlist ')' newlines funcbody
  {
    AST *funcdecl = NewAST(AST_FUNCDECL, $2, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $4, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    DeclareFunction(1, funcdef, $7, NULL, $1);
  }
  ;

funcbody:
  statementlist BAS_END BAS_SUB
  { $$ = $1; }
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
