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
%token BAS_INTEGER    "integer number"
%token BAS_FLOAT      "number"
%token BAS_STRING     "literal string"
%token BAS_EOLN       "end of line"
%token BAS_EOF        "end of file"

/* keywords */
%token BAS_AND        "and"
%token BAS_AS         "as"
%token BAS_ASM        "asm"
%token BAS_BYTE       "byte"
%token BAS_CONTINUE   "continue"
%token BAS_DECLARE    "declare"
%token BAS_DIM        "dim"
%token BAS_DO         "do"
%token BAS_ELSE       "else"
%token BAS_END        "end"
%token BAS_EXIT       "exit"
%token BAS_FOR        "for"
%token BAS_FUNCTION   "function"
%token BAS_GOTO       "goto"
%token BAS_IF         "if"
%token BAS_INTEGER_KW "integer"
%token BAS_LET        "let"
%token BAS_LOCAL      "local"
%token BAS_LONG       "long"
%token BAS_LOOP       "loop"
%token BAS_MOD        "mod"
%token BAS_NEXT       "next"
%token BAS_OR         "or"
%token BAS_PRINT      "print"
%token BAS_PROGRAM    "program"
%token BAS_REAL       "real"
%token BAS_RETURN     "return"
%token BAS_STEP       "step"
%token BAS_STRING_KW  "string"
%token BAS_STRUCT     "struct"
%token BAS_SUB        "sub"
%token BAS_THEN       "then"
%token BAS_TO         "to"
%token BAS_UNTIL      "until"
%token BAS_WEND       "wend"
%token BAS_WHILE      "while"
%token BAS_WORD       "word"
%token BAS_LE         "<="
%token BAS_GE         ">="
%token BAS_NE         "<>"

%left BAS_OR
%left BAS_AND
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
  statement
  | topdecl
  | newlines
;

statement:
  lhs '=' expr newlines
    { $$ = AstAssign($1, $3); }
  | BAS_LET BAS_IDENTIFIER '=' expr newlines
    { MaybeDeclareGlobal(current, $2, InferTypeFromName($2));
      $$ = AstAssign($2, $4); }
  | BAS_LOCAL BAS_IDENTIFIER newlines
    { $$ = AstDeclareLocal($2, NULL); }
  | BAS_LOCAL BAS_IDENTIFIER '=' expr newlines
    { $$ = AstDeclareLocal($2, $4); }
  | BAS_RETURN newlines
    { $$ = AstReturn(NULL, $1); }
  | BAS_RETURN expr newlines
    { $$ = AstReturn($2, $1); }
  | ifstmt
    { $$ = $1; }
  | whilestmt
    { $$ = $1; }
  | doloopstmt
    { $$ = $1; }
;

ifstmt:
  BAS_IF boolexpr BAS_THEN newlines elseblock
    { $$ = NewCommentedAST(AST_IF, $2, $5, $1); }
;
elseblock:
  statementlist endif
    { $$ = NewAST(AST_THENELSE, $1, NULL); }
  | statementlist BAS_ELSE newlines statementlist endif
    { $$ = NewAST(AST_THENELSE, $1, $4); }
;

endif:
  BAS_END newlines
  | BAS_END BAS_IF newlines
;

whilestmt:
  BAS_WHILE boolexpr newlines statementlist endwhile
    { AST *body = CheckYield($4);
      $$ = NewCommentedAST(AST_WHILE, $2, body, $1);
    }
;

endwhile:
  BAS_WEND newlines
  | BAS_END BAS_WHILE newlines
  ;

doloopstmt:
  BAS_DO newlines statementlist BAS_LOOP newlines
    { AST *body = CheckYield($3);
      AST *one = AstInteger(1);
      $$ = NewCommentedAST(AST_WHILE, one, body, $1);
    }
  | BAS_DO newlines statementlist BAS_LOOP BAS_WHILE boolexpr newlines
    { $$ = NewCommentedAST(AST_DOWHILE, $6, CheckYield($3), $1); }
  | BAS_DO newlines statementlist BAS_LOOP BAS_UNTIL boolexpr newlines
    { $$ = NewCommentedAST(AST_DOWHILE, AstOperator(K_BOOL_NOT, NULL, $6), CheckYield($3), $1); }
  ;

statementlist:
  statement
    { $$ = NewCommentedStatement($1); }
  | statementlist statement
    { $$ = AddToList($1, NewCommentedStatement($2)); }
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
;

boolexpr:
  expr '=' expr
    { $$ = AstOperator(K_EQ, $1, $3); }
  | expr BAS_NE expr
    { $$ = AstOperator(K_NE, $1, $3); }
  | expr BAS_LE expr
    { $$ = AstOperator(K_LE, $1, $3); }
  | expr BAS_GE expr
    { $$ = AstOperator(K_GE, $1, $3); }
  | expr '<' expr
    { $$ = AstOperator('<', $1, $3); }
  | expr '>' expr
    { $$ = AstOperator('>', $1, $3); }
  | boolexpr BAS_AND boolexpr
    { $$ = AstOperator(K_BOOL_AND, $1, $3); }
  | boolexpr BAS_OR boolexpr
    { $$ = AstOperator(K_BOOL_OR, $1, $3); }
  | '(' boolexpr ')'
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
  subdecl
  | funcdecl
  ;

subdecl:
  BAS_SUB BAS_IDENTIFIER '(' identifierlist ')' newlines subbody
  {
    AST *funcdecl = NewAST(AST_FUNCDECL, $2, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $4, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    DeclareFunction(1, funcdef, $7, NULL, $1);
  }
  ;

funcdecl:
  BAS_FUNCTION BAS_IDENTIFIER '(' identifierlist ')' newlines funcbody
  {
    AST *funcdecl = NewAST(AST_FUNCDECL, $2, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $4, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    DeclareFunction(1, funcdef, $7, NULL, $1);
  }
  ;

subbody:
  statementlist BAS_END BAS_SUB newlines
  { $$ = $1; }
  ;

funcbody:
  statementlist BAS_END BAS_FUNCTION newlines
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
