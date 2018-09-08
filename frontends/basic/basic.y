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

AST *GetIORegister(const char *name)
{
    Symbol *sym = FindSymbol(&basicReservedWords, name);
    AST *reg = NewAST(AST_HWREG, NULL, NULL);
    if (!sym) {
        ERROR(NULL, "Unknown ioregister %s", name);
        return NULL;
    }
    reg->d.ptr = sym->val;
    return reg;
}

AST *
DeclareBasicVariables(AST *idlist, AST *typ)
{
    AST *ident;
    
    while (idlist) {
        ident = idlist->left;
        MaybeDeclareGlobal(current, ident, typ);
        idlist = idlist->right;
    }
    return NULL;
}

AST *BASICArrayRef(AST *id, AST *expr)
{
    AST *ast;

    // BASIC arrays start at 1
    expr = AstOperator('-', expr, AstInteger(1));
    ast = NewAST(AST_ARRAYREF, id, expr);
    return ast;
}
%}

%pure-parser

%token BAS_IDENTIFIER "identifier"
%token BAS_INTEGER    "integer number"
%token BAS_FLOAT      "number"
%token BAS_STRING     "literal string"
%token BAS_EOLN       "end of line"
%token BAS_EOF        "end of file"
%token BAS_TYPENAME   "class name"

/* keywords */
%token BAS_AND        "and"
%token BAS_AS         "as"
%token BAS_ASM        "asm"
%token BAS_BYTE       "byte"
%token BAS_CLASS      "class"
%token BAS_CONTINUE   "continue"
%token BAS_DECLARE    "declare"
%token BAS_DIM        "dim"
%token BAS_DIRECTION  "direction"
%token BAS_DO         "do"
%token BAS_ELSE       "else"
%token BAS_END        "end"
%token BAS_ENUM       "enum"
%token BAS_EXIT       "exit"
%token BAS_FOR        "for"
%token BAS_FUNCTION   "function"
%token BAS_GOTO       "goto"
%token BAS_IF         "if"
%token BAS_INPUT      "input"
%token BAS_INTEGER_KW "integer"
%token BAS_LET        "let"
%token BAS_LOCAL      "local"
%token BAS_LONG       "long"
%token BAS_LOOP       "loop"
%token BAS_MOD        "mod"
%token BAS_NEXT       "next"
%token BAS_NOT        "not"
%token BAS_OR         "or"
%token BAS_OUTPUT     "output"
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
%token BAS_WITH       "with"
%token BAS_WHILE      "while"
%token BAS_WORD       "word"
%token BAS_XOR        "xor"
%token BAS_LE         "<="
%token BAS_GE         ">="
%token BAS_NE         "<>"
%token BAS_NEGATE     "-"

%left BAS_OR BAS_XOR
%left BAS_AND
%left '<' '>' BAS_LE BAS_GE BAS_NE '='
%left '-' '+'
%left '*' '/' BAS_MOD
%left BAS_NEGATE

%%
toplist:
 /* empty */
 | toplist1
;

toplist1:
 topstatement
 | toplist1 topstatement
;

eoln:
  BAS_EOLN
  ;

topstatement:
  statement
    {
        AST *stmtholder = NewCommentedStatement($1);
        current->body = AddToList(current->body, stmtholder);
    }
  | topdecl
;

statement:
  BAS_IDENTIFIER ':'
    { $$ = NewAST(AST_LABEL, $1, NULL); }
  | BAS_IDENTIFIER '=' expr eoln
    { $$ = AstAssign($1, $3); }
  | BAS_IDENTIFIER '(' expr ')' '=' expr eoln
    { $$ = AstAssign(BASICArrayRef($1, $3), $6); }
  | BAS_OUTPUT '(' expr ')' '=' expr eoln
    {
        AST *outa = GetIORegister("outa");
        AST *lhs;
        lhs = NewAST(AST_RANGEREF, outa, NewAST(AST_RANGE, $3, NULL));
        $$ = AstAssign(lhs, $6);
    }
  | BAS_DIRECTION '(' expr ')' '=' expr eoln
    {
        AST *reg = GetIORegister("dira");
        AST *lhs;
        lhs = NewAST(AST_RANGEREF, reg, NewAST(AST_RANGE, $3, NULL));
        $$ = AstAssign(lhs, $6);
    }
  | BAS_LET BAS_IDENTIFIER '=' expr eoln
    { MaybeDeclareGlobal(current, $2, InferTypeFromName($2));
      $$ = AstAssign($2, $4); }
  | BAS_IDENTIFIER '(' optexprlist ')' eoln
    { $$ = NewAST(AST_FUNCCALL, $1, $3); }
  | BAS_IDENTIFIER '.' BAS_IDENTIFIER '(' optexprlist ')' eoln
    { $$ = NewAST(AST_FUNCCALL, NewAST(AST_METHODREF, $1, $3), $5); }
  | BAS_IDENTIFIER optexprlist eoln
    { $$ = NewAST(AST_FUNCCALL, $1, $2); }
  | BAS_IDENTIFIER eoln
    { $$ = NewAST(AST_FUNCCALL, $1, NULL); }
  | BAS_LOCAL BAS_IDENTIFIER eoln
    { $$ = AstDeclareLocal($2, NULL); }
  | BAS_LOCAL BAS_IDENTIFIER '=' expr eoln
    { $$ = AstDeclareLocal($2, $4); }
  | BAS_RETURN eoln
    { $$ = AstReturn(NULL, $1); }
  | BAS_RETURN expr eoln
    { $$ = AstReturn($2, $1); }
  | BAS_GOTO BAS_IDENTIFIER eoln
    { $$ = NewAST(AST_GOTO, $2, NULL); }
  | BAS_PRINT exprlist eoln
    { $$ = NewAST(AST_PRINT, $2, NULL); }
  | ifstmt
    { $$ = $1; }
  | whilestmt
    { $$ = $1; }
  | doloopstmt
    { $$ = $1; }
  | forstmt
    { $$ = $1; }
  | eoln
    { $$ = NULL; }
;

ifstmt:
  BAS_IF expr BAS_THEN eoln elseblock
    { $$ = NewCommentedAST(AST_IF, $2, $5, $1); }
  | BAS_IF expr statement
    {
        AST *stmtlist = NewCommentedStatement($3);
        AST *elseblock = NewAST(AST_THENELSE, stmtlist, NULL);
        $$ = NewCommentedAST(AST_IF, $2, elseblock, $1);
    }
;
elseblock:
  statementlist endif
    { $$ = NewAST(AST_THENELSE, $1, NULL); }
  | statementlist BAS_ELSE eoln statementlist endif
    { $$ = NewAST(AST_THENELSE, $1, $4); }
;

endif:
  BAS_END eoln
  | BAS_END BAS_IF eoln
;

whilestmt:
  BAS_WHILE expr eoln statementlist endwhile
    { AST *body = CheckYield($4);
      $$ = NewCommentedAST(AST_WHILE, $2, body, $1);
    }
;

endwhile:
  BAS_WEND eoln
  | BAS_END BAS_WHILE eoln
  ;

doloopstmt:
  BAS_DO eoln statementlist BAS_LOOP eoln
    { AST *body = CheckYield($3);
      AST *one = AstInteger(1);
      $$ = NewCommentedAST(AST_WHILE, one, body, $1);
    }
  | BAS_DO eoln statementlist BAS_LOOP BAS_WHILE expr eoln
    { $$ = NewCommentedAST(AST_DOWHILE, $6, CheckYield($3), $1); }
  | BAS_DO eoln statementlist BAS_LOOP BAS_UNTIL expr eoln
    { $$ = NewCommentedAST(AST_DOWHILE, AstOperator(K_BOOL_NOT, NULL, $6), CheckYield($3), $1); }
  ;

forstmt:
  BAS_FOR BAS_IDENTIFIER '=' expr BAS_TO expr optstep eoln statementlist endfor
    {
      AST *from, *to, *step;
      AST *ident = $2;
      AST *closeident = $10;
      MaybeDeclareGlobal(current, ident, InferTypeFromName(ident));
      step = NewAST(AST_STEP, $7, $9);
      to = NewAST(AST_TO, $6, step);
      from = NewAST(AST_FROM, $4, to);
      $$ = NewCommentedAST(AST_COUNTREPEAT, $2, from, $1);
      // validate the "next i"
      if (closeident && !AstMatch(ident, closeident)) {
          fprintf(stderr, "%s:%d: error: ", current->L.fileName, current->L.lineCounter);
          fprintf(stderr, "Wrong variable in next: expected %s, saw %s\n", ident->d.string, closeident->d.string);
      }
    }
;

endfor:
  BAS_NEXT eoln
    { $$ = NULL; }
  | BAS_NEXT BAS_IDENTIFIER eoln
    { $$ = $2; }
;

optstep:
  /* nothing */
    { $$ = AstInteger(1); }
  | BAS_STEP expr
    { $$ = $2; }
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

iorange:
  BAS_OUTPUT '(' expr ')'
  {   AST *outa = GetIORegister("outa");
      $$ = NewAST(AST_RANGEREF, outa, NewAST(AST_RANGE, $3, NULL));
  }
;

exprlist:
  expritem
 | exprlist ',' expritem
   { $$ = AddToList($1, $3); }
 ;

optexprlist:
  /* empty */
    {  $$ = NULL; }
  | exprlist
    { $$ = $1; }
;

expritem:
  expr
   { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
;

expr:
  BAS_INTEGER
    { $$ = $1; }
  | BAS_FLOAT
    { $$ = $1; }
  | BAS_STRING
    { $$ = NewAST(AST_STRINGPTR,
                  NewAST(AST_EXPRLIST, $1, NULL), NULL); }
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
  | expr '=' expr
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
  | expr BAS_AND expr
    { $$ = AstOperator('&', $1, $3); }
  | expr BAS_OR expr
    { $$ = AstOperator('|', $1, $3); }
  | expr BAS_XOR expr
    { $$ = AstOperator('^', $1, $3); }
  | '-' expr %prec BAS_NEGATE
    { $$ = AstOperator(K_NEGATE, NULL, $2); }
  | BAS_NOT expr
    { $$ = AstOperator(K_BIT_NOT, NULL, $2); } 
  | BAS_IDENTIFIER '(' optexprlist ')'
    { $$ = NewAST(AST_FUNCCALL, $1, $3); }
  | BAS_IDENTIFIER '.' BAS_IDENTIFIER '(' optexprlist ')'
    { 
        $$ = NewAST(AST_FUNCCALL, NewAST(AST_METHODREF, $1, $3), $5);
    }
  | '(' expr ')'
    { $$ = $2; }
;

lhs: identifier
    { $$ = $1; }
;

identifier:
  BAS_IDENTIFIER
    { $$ = $1; }
;

topdecl:
  subdecl
  | funcdecl
  | classdecl
  | dimension
  | pindecl
  ;

subdecl:
  BAS_SUB BAS_IDENTIFIER '(' identifierlist ')' eoln subbody
  {
    AST *funcdecl = NewAST(AST_FUNCDECL, $2, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $4, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    DeclareFunction(ast_type_void, 1, funcdef, $7, NULL, $1);
  }
  | BAS_SUB BAS_IDENTIFIER eoln subbody
  {
    AST *funcdecl = NewAST(AST_FUNCDECL, $2, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, NULL, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    DeclareFunction(ast_type_void, 1, funcdef, $4, NULL, $1);
  }
  ;

funcdecl:
  BAS_FUNCTION BAS_IDENTIFIER '(' identifierlist ')' eoln funcbody
  {
    AST *name = $2;
    AST *funcdecl = NewAST(AST_FUNCDECL, name, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $4, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    AST *rettype = InferTypeFromName(name);
    DeclareFunction(rettype, 1, funcdef, $7, NULL, $1);
  }
  ;

subbody:
  statementlist BAS_END BAS_SUB eoln
  { $$ = $1; }
  ;

funcbody:
  statementlist BAS_END BAS_FUNCTION eoln
  { $$ = $1; }
  ;

classdecl:
  BAS_CLASS BAS_IDENTIFIER BAS_INPUT BAS_STRING eoln
    {
        AST *newobj = NewAbstractObject( $2, $4 );
        DeclareObjects(newobj);
        current->objblock = AddToList(current->objblock, newobj);
        $$ = NULL;
    }
  ;

dimension:
  BAS_DIM identlist BAS_AS typename
    { $$ = DeclareBasicVariables($2, $4); }
   | BAS_DIM BAS_AS typename identlist
    { $$ = DeclareBasicVariables($4, $3); }
   | BAS_DIM identlist
    { $$ = DeclareBasicVariables($2, NULL); } 
  ;

pindecl:
  BAS_DECLARE BAS_IDENTIFIER BAS_AS iorange
    { ERROR($1, "pin declarations not implemented yet"); }
  ;

identdecl:
  identifier
    { $$ = $1; }
  | identifier '(' expr ')'
    { $$ = NewAST(AST_ARRAYDECL, $1, $3); }
;

identlist:
  identdecl
  { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
  | identlist ',' identdecl
  { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $3, NULL)); }
  ;

typename:
  BAS_BYTE
    { $$ = ast_type_byte; }
  | BAS_WORD
    { $$ = ast_type_word; }
  | BAS_LONG
    { $$ = ast_type_long; }
  | BAS_INTEGER_KW
    { $$ = ast_type_long; }
  | BAS_REAL
    { $$ = ast_type_float; }
  | BAS_STRING_KW
    { $$ = ast_type_string; }
  | BAS_TYPENAME
    { $$ = $1; }
;

%%
void
basicyyerror(const char *msg)
{
    extern int saved_basicyychar;
    int yychar = saved_basicyychar;
    
    ERRORHEADER(current->L.fileName, current->L.lineCounter);

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
