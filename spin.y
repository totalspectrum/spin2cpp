/*
 * Spin compiler parser
 * Copyright (c) 2011-2014 Total Spectrum Software Inc.
 * See the file COPYING for terms of use.
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include "spinc.h"

/* Yacc functions */
    void yyerror(char *);
    int yylex();

    extern int gl_errors;

/* Skip Comments */
void
SkipComments(void)
{
    (void)GetComments();
}

/* create an AST in a comment holder */
AST *
NewCommentedAST(enum astkind kind, AST *left, AST *right, AST *comment)
{
    AST *ast;

    ast = NewAST(kind, left, right);
    if (comment) {
        ast = NewAST(AST_COMMENTEDNODE, ast, comment);
    }
    return ast;
}

/* add a list element together with accumulated comments */
AST *
CommentedListHolder(AST *ast)
{
    AST *comment;

    if (!ast)
        return ast;

    comment = GetComments();

    if (comment) {
        ast = NewAST(AST_COMMENTEDNODE, ast, comment);
    }
    ast = NewAST(AST_LISTHOLDER, ast, NULL);
    return ast;
}

AST *
NewStatement(AST *stmt)
{
    AST *ast;

    if (!stmt) return NULL;
    ast = NewAST(AST_STMTLIST, stmt, NULL);
    return ast;
}

AST *
NewCommentedStatement(AST *stmt)
{
    AST *ast;
    AST *comment;

    if (!stmt) return NULL;
    comment = GetComments();
    if (comment) {
        stmt = NewAST(AST_COMMENTEDNODE, stmt, comment);
    }
    ast = NewAST(AST_STMTLIST, stmt, NULL);
    return ast;
}

/* utility functions */
AST *
AstYield(void)
{
    current->needsYield = 1;
    return NewStatement(NewAST(AST_YIELD, NULL, NULL));
}

AST *
AstAbort(AST *expr, AST *comment)
{
    current->needsAbortdef = 1;
    current->needsStdlib = 1;
    return NewCommentedAST(AST_ABORT, expr, NULL, comment);
}

AST *
AstCatch(AST *expr)
{
    current->needsAbortdef = 1;
    return NewAST(AST_CATCH, expr, NULL);
}

/* determine whether a loop needs a yield, and if so, insert one */
AST *
CheckYield(AST *body)
{
    AST *ast = body;

    if (!body)
        return AstYield();
    while (ast) {
        if (ast->left)
            return body;
        ast = ast->right;
    }
    return AddToList(body, AstYield());
}

%}

%pure-parser
%token T_IDENTIFIER
%token T_NUM
%token T_STRING
%token T_FLOATNUM

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

%token T_INSTR
%token T_INSTRMODIFIER
%token T_HWREG
%token T_ORG
%token T_RES
%token T_FIT

%token T_REPEAT
%token T_FROM
%token T_TO
%token T_STEP
%token T_WHILE
%token T_UNTIL
%token T_IF
%token T_IFNOT
%token T_ELSE
%token T_ELSEIF
%token T_ELSEIFNOT

%token T_LOOKDOWN
%token T_LOOKDOWNZ
%token T_LOOKUP
%token T_LOOKUPZ
%token T_COGINIT
%token T_COGNEW

%token T_CASE
%token T_OTHER

%token T_QUIT
%token T_NEXT

/* other stuff */
%token T_ABORT
%token T_RESULT
%token T_RETURN
%token T_INDENT
%token T_OUTDENT
%token T_EOLN
%token T_EOF
%token T_DOTS
%token T_HERE
%token T_STRINGPTR
%token T_FILE

%token T_ANNOTATION

/* operators */
%right T_ASSIGN
%left T_OR
%left T_AND
%left '<' '>' T_GE T_LE T_NE T_EQ
%left T_LIMITMIN T_LIMITMAX
%left '-' '+'
%left '*' '/' T_MODULUS T_HIGHMULT
%left '|' '^'
%left '&'
%left T_ROTL T_ROTR T_SHL T_SHR T_SAR T_REV
%left T_NEGATE T_BIT_NOT T_ABS T_SQRT T_DECODE T_ENCODE
%left T_NOT
%left '@' '~' '?' T_DOUBLETILDE T_INCREMENT T_DECREMENT T_DOUBLEAT
%left T_CONSTANT T_FLOAT T_TRUNC T_ROUND

%%
input:
  emptyline
  | emptyline input
  | rest
;

rest:
  topelement
  | topelement rest
  ;

emptyline:
  T_EOLN
  ;

emptylines: 
  | emptylines emptyline
  ;

topelement:
  T_CON conblock
  { $$ = current->conblock = AddToList(current->conblock, $2); }
  | T_DAT datblock
  { $$ = current->datblock = AddToList(current->datblock, $2); }
  | T_DAT annotation datblock
  { 
      current->datannotations = AddToList(current->datannotations, $2);
      $$ = current->datblock = AddToList(current->datblock, $3); 
  }
  | T_VAR varblock
  { $$ = current->varblock = AddToList(current->varblock, $2); }
  | T_OBJ objblock
  { DeclareObjects($2);
    $$ = current->objblock = AddToList(current->objblock, $2); }
  | T_PUB funcdef funcbody
  { DeclareFunction(1, $2, $3, NULL, $1); }
  | T_PRI funcdef funcbody
  { DeclareFunction(0, $2, $3, NULL, $1); }
  | T_PUB annotation funcdef funcbody
  { DeclareFunction(1, $3, $4, $2, $1); }
  | T_PRI annotation funcdef funcbody
  { DeclareFunction(0, $3, $4, $2, $1); }
  | annotation emptylines
  { DeclareAnnotation($1); }
;

funcdef:
  identifier optparamlist T_EOLN
  { AST *funcdecl = NewAST(AST_FUNCDECL, $1, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $2, NULL);
    $$ = NewAST(AST_FUNCDEF, funcdecl, funcvars);
  }
|  identifier optparamlist localvars T_EOLN
  { AST *funcdecl = NewAST(AST_FUNCDECL, $1, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $2, $3);
    $$ = NewAST(AST_FUNCDEF, funcdecl, funcvars);
  }
|  identifier optparamlist resultname localvars T_EOLN
  { AST *funcdecl = NewAST(AST_FUNCDECL, $1, $3);
    AST *funcvars = NewAST(AST_FUNCVARS, $2, $4);
    $$ = NewAST(AST_FUNCDEF, funcdecl, funcvars);
  }
|  identifier optparamlist resultname T_EOLN
  { AST *funcdecl = NewAST(AST_FUNCDECL, $1, $3);
    AST *funcvars = NewAST(AST_FUNCVARS, $2, NULL);
    $$ = NewAST(AST_FUNCDEF, funcdecl, funcvars);
  }
;

optparamlist:
/* empty */
  { $$ = NULL; }
| identlist
  { $$ = $1; }
| '(' identlist ')'
  { $$ = $2; }
  ;

resultname: ':' identifier
  { $$ = $2; }
  ;

localvars:
 '|' identlist
  { $$ = $2; }
    ;

funcbody:
  /* empty */
  { $$ = NULL; }
| stmtlist
  { $$ = $1; }
  ;

stmtlist:
  stmt
    {
        $$ = $1;
    }
  | stmtlist stmt
  {
      $$ = AddToList($1, $2); 
  }
  ;

stmt:
  basicstmt
    {  $$ = NewCommentedStatement($1); }
  | compoundstmt
    { $$ = NewStatement($1); }
  | T_EOLN
    { $$ = NULL; }
  | error T_EOLN
    { $$ = NULL; }
  ;
  ;

basicstmt:
   T_RETURN T_EOLN
   { $$ = NewCommentedAST(AST_RETURN, NULL, NULL, $1); }
  |  T_RETURN expr T_EOLN
  { $$ = NewCommentedAST(AST_RETURN, $2, NULL, $1); }
  | T_ABORT T_EOLN
    { $$ = AstAbort(NULL, $1); }
  |  T_ABORT expr T_EOLN
    { $$ = AstAbort($2, $1); }
  |  expr T_EOLN
    { $$ = $1; }
  | T_QUIT T_EOLN
    { $$ = NewCommentedAST(AST_QUIT, NULL, NULL, $1); }
  | T_NEXT T_EOLN
    { $$ = NewCommentedAST(AST_NEXT, NULL, NULL, $1); }

;

compoundstmt:
   ifstmt
    { $$ = $1; }
  | repeatstmt
    { $$ = $1; }
  | stmtblock
    { $$ = $1; }
   | casestmt
    { $$ = $1; }
;

stmtblock:
  T_INDENT stmtlist T_OUTDENT
  { $$ = $2; }
  | T_INDENT T_OUTDENT
  { $$ = NULL; }
;

ifstmt:
  T_IF expr T_EOLN elseblock
    { $$ = NewCommentedAST(AST_IF, $2, $4, $1); }
  | T_IFNOT expr T_EOLN elseblock
    { $$ = NewCommentedAST(AST_IF, AstOperator(T_NOT, NULL, $2), $4, $1); }
;

elseblock:
  stmtblock
    { $$ = NewAST(AST_THENELSE, $1, NULL); }
  | stmtblock T_ELSE T_EOLN stmtblock
  { $$ = NewCommentedAST(AST_THENELSE, $1, $4, $2); }
  | stmtblock T_ELSEIF expr T_EOLN elseblock
    { $$ = NewAST(AST_THENELSE, $1, NewAST(AST_STMTLIST, NewCommentedAST(AST_IF, $3, $5, $2), NULL)); }
  | stmtblock T_ELSEIFNOT expr T_EOLN elseblock
    { $$ = NewAST(AST_THENELSE, $1, NewAST(AST_STMTLIST, NewCommentedAST(AST_IF, AstOperator(T_NOT, NULL, $3), $5, $2), NULL)); }
  ;

casestmt:
  T_CASE expr T_EOLN T_INDENT casematchlist T_OUTDENT
    { $$ = NewCommentedAST(AST_CASE, $2, $5, $1); }
;

casematchlist:
  casematchitem
    { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
  | casematchlist casematchitem
    { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $2, NULL)); }
  ;

casematchitem:
  casematch T_EOLN stmtblock
    {
        AST *slist = NewAST(AST_STMTLIST, $3, NULL);
        $$ = NewAST(AST_CASEITEM, $1, slist);
    }
  ;

casematch:
  matchexprlist ':'
  {
      $$ = $1;
      EstablishIndent(&current->L, -1);
      resetLineState(&current->L);
  }

matchexprlist:
  matchexpritem
  | matchexprlist ',' matchexpritem
    { $$ = AddToList($1, $3); }
  ;

matchexpritem:
  T_OTHER
    { $$ = NewAST(AST_OTHER, NULL, NULL); }
  | expr T_DOTS expr
    { $$ = NewAST(AST_EXPRLIST, NewAST(AST_RANGE, $1, $3), NULL); current->needsBetween = 1; }
  | expr
    { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  ;


rangeexpritem:
  expr
    { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  | expr T_DOTS expr
    { $$ = NewAST(AST_EXPRLIST, NewAST(AST_RANGE, $1, $3), NULL); }
  ;

rangeexprlist:
  rangeexpritem
  | rangeexprlist ',' rangeexpritem
    { $$ = AddToList($1, $3); }
  ;

repeatstmt:
    T_REPEAT T_EOLN stmtblock
    {   AST *body = $3; body = CheckYield(body); 
        $$ = NewCommentedAST(AST_WHILE, AstInteger(1), body, $1); }
  | T_REPEAT T_EOLN stmtblock T_WHILE expr T_EOLN
    { $$ = NewCommentedAST(AST_DOWHILE, $5, CheckYield($3), $1); }
  | T_REPEAT T_EOLN stmtblock T_UNTIL expr T_EOLN
    { $$ = NewCommentedAST(AST_DOWHILE, AstOperator(T_NOT, NULL, $5), CheckYield($3), $1); }
  | T_REPEAT T_WHILE expr T_EOLN stmtblock
    {   AST *body = $5; body = CheckYield(body); 
        $$ = NewCommentedAST(AST_WHILE, $3, body, $1); }
  | T_REPEAT T_UNTIL expr T_EOLN stmtblock
    {   AST *body = $5; body = CheckYield(body); 
        $$ = NewCommentedAST(AST_WHILE, AstOperator(T_NOT, NULL, $3), body, $1); }
  | T_REPEAT identifier T_FROM expr T_TO expr T_STEP expr T_EOLN stmtblock
    {
      AST *from, *to, *step; 
      step = NewAST(AST_STEP, $8, $10);
      to = NewAST(AST_TO, $6, step);
      from = NewAST(AST_FROM, $4, to);
      $$ = NewCommentedAST(AST_COUNTREPEAT, $2, from, $1);
    }
  | T_REPEAT identifier T_FROM expr T_TO expr T_EOLN stmtblock
    {
      AST *from, *to, *step; 
      step = NewAST(AST_STEP, AstInteger(1), $8);
      to = NewAST(AST_TO, $6, step);
      from = NewAST(AST_FROM, $4, to);
      $$ = NewCommentedAST(AST_COUNTREPEAT, $2, from, $1);
    }
  | T_REPEAT expr T_EOLN stmtblock
    {
      AST *from, *to, *step;
      AST *body = $4;
      body = CheckYield(body);
      step = NewAST(AST_STEP, AstInteger(1), body);
      to = NewAST(AST_TO, $2, step);
      from = NewAST(AST_FROM, NULL, to);
      $$ = NewCommentedAST(AST_COUNTREPEAT, NULL, from, $1);
    }

;

lookupexpr:
  T_LOOKUPZ '(' expr ':' rangeexprlist ')'
    { $$ = AstLookup(AST_LOOKUP, 0, $3, $5); }
  | T_LOOKUP '(' expr ':' rangeexprlist ')'
    { $$ = AstLookup(AST_LOOKUP, 1, $3, $5); }
;
lookdownexpr:
  T_LOOKDOWNZ '(' expr ':' rangeexprlist ')'
    { $$ = AstLookup(AST_LOOKDOWN, 0, $3, $5); }
  | T_LOOKDOWN '(' expr ':' rangeexprlist ')'
    { $$ = AstLookup(AST_LOOKDOWN, 1, $3, $5); }
;

conblock:
  conline
  { $$ = $1; }
  | conblock conline
  { $$ = AddToList($1, $2); }
  ;

conline:
  enumlist T_EOLN
    { $$ = $1; }
  | T_EOLN
    { $$ = NULL; }
  | error T_EOLN
    { $$ = NULL; }
  ;

enumlist:
  enumitem
    { $$ = CommentedListHolder($1); }
  | enumlist ',' enumitem
    { $$ = AddToList($1, CommentedListHolder($3)); }
  ;

enumitem:
  identifier '=' expr
  { $$ = NewAST(AST_ASSIGN, $1, $3); }
  | identifier
  { $$ = $1; }
  | identifier '[' expr ']'
    {
        $$ = NewAST(AST_ENUMSKIP, $1, $3);
    }
  | '#' expr
  { $$ = NewAST(AST_ENUMSET, $2, NULL); }
  ;

datblock:
  datline
    { $$ = $1; SkipComments(); }
  | datblock datline
    { $$ = AddToList($1, $2); }
  ;

datline:
  basedatline
  | identifier basedatline
    {   AST *linebreak = NewAST(AST_LINEBREAK, NULL, NULL);
        $$ = AddToList(linebreak, AddToList($1, $2)); 
    }
  ;

basedatline:
  T_EOLN
    { $$ = NULL; }
  | error T_EOLN
    { $$ = NULL; }
  | T_BYTE T_EOLN
    { $$ = NewAST(AST_BYTELIST, NULL, NULL); }
  | T_BYTE exprlist T_EOLN
    { $$ = NewAST(AST_BYTELIST, $2, NULL); }
  | T_WORD T_EOLN
    { $$ = NewAST(AST_WORDLIST, NULL, NULL); }
  | T_WORD exprlist T_EOLN
    { $$ = NewAST(AST_WORDLIST, $2, NULL); }
  | T_LONG T_EOLN
    { $$ = NewAST(AST_LONGLIST, NULL, NULL); }
  | T_LONG exprlist T_EOLN
    { $$ = NewAST(AST_LONGLIST, $2, NULL); }
  | instruction T_EOLN
    { $$ = NewAST(AST_INSTRHOLDER, $1, NULL); }
  | instruction operandlist T_EOLN
    { $$ = NewAST(AST_INSTRHOLDER, AddToList($1, $2), NULL); }
  | instruction modifierlist T_EOLN
    { $$ = NewAST(AST_INSTRHOLDER, AddToList($1, $2), NULL); }
  | instruction operandlist modifierlist T_EOLN
    { $$ = NewAST(AST_INSTRHOLDER, AddToList($1, AddToList($2, $3)), NULL); }
  | T_ORG T_EOLN
    { $$ = NewAST(AST_ORG, NULL, NULL); }
  | T_ORG expr T_EOLN
    { $$ = NewAST(AST_ORG, $2, NULL); }
  | T_RES expr T_EOLN
    { $$ = NewAST(AST_RES, $2, NULL); }
  | T_FIT expr T_EOLN
    { $$ = NewAST(AST_FIT, $2, NULL); }
  | T_FIT T_EOLN
    { $$ = NewAST(AST_FIT, AstInteger(0x1f0), NULL); }
  | T_FILE string T_EOLN
    { $$ = NewAST(AST_FILE, $2, NULL); }
  ;

objblock:
  objline
  { $$ = $1; }
  | objblock objline
  { $$ = AddToList($1, $2); }
;

objline:
    T_EOLN
    { $$ = NULL; }
  | error  T_EOLN
    { $$ = NULL; }
  | identdecl ':' string
    { $$ = NewObject($1, $3); }
;

varblock:
    varline
    { $$ = CommentedListHolder($1); }
  | varblock varline
    { $$ = AddToList($1, CommentedListHolder($2)); }
  ;

varline:
  T_BYTE identlist T_EOLN
    { $$ = NewAST(AST_BYTELIST, $2, NULL); }
  | T_WORD identlist T_EOLN
    { $$ = NewAST(AST_WORDLIST, $2, NULL); }
  | T_LONG identlist T_EOLN
    { $$ = NewAST(AST_LONGLIST, $2, NULL); }
  | T_EOLN
    { $$ = NULL; }
  | error T_EOLN
    { $$ = NULL; }
  ;

identlist:
  identdecl
  { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
  | annotation identdecl
  { $$ = AddToList(NewAST(AST_LISTHOLDER, $1, NULL),
                   NewAST(AST_LISTHOLDER, $2, NULL)); }
  | identlist ',' identdecl
  { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $3, NULL)); }
  ;

identdecl:
  identifier
  { $$ = $1; }
  | identifier '[' expr ']'
  { $$ = NewAST(AST_ARRAYDECL, $1, $3); }
  ;


expr:
  integer
  | float
  | string
  | T_STRINGPTR '(' exprlist ')'
    { $$ = NewAST(AST_STRINGPTR, $3, NULL); }  
  | lhs
  | '@' lhs
    { $$ = NewAST(AST_ADDROF, $2, NULL); }
  | T_DOUBLEAT lhs
    { $$ = NewAST(AST_DATADDROF, $2, NULL); }
  | lhs T_ASSIGN expr
    { $$ = AstAssign(T_ASSIGN, $1, $3); }
  | identifier '#' identifier
    { $$ = NewAST(AST_CONSTREF, $1, $3); }
  | expr '+' expr
    { $$ = AstOperator('+', $1, $3); }
  | expr '-' expr
    { $$ = AstOperator('-', $1, $3); }
  | expr '*' expr
    { $$ = AstOperator('*', $1, $3); }
  | expr '/' expr
    { $$ = AstOperator('/', $1, $3); }
  | expr '&' expr
    { $$ = AstOperator('&', $1, $3); }
  | expr '|' expr
    { $$ = AstOperator('|', $1, $3); }
  | expr '^' expr
    { $$ = AstOperator('^', $1, $3); }
  | expr '>' expr
    { $$ = AstOperator('>', $1, $3); }
  | expr '<' expr
    { $$ = AstOperator('<', $1, $3); }
  | expr T_GE expr
    { $$ = AstOperator(T_GE, $1, $3); }
  | expr T_LE expr
    { $$ = AstOperator(T_LE, $1, $3); }
  | expr T_NE expr
    { $$ = AstOperator(T_NE, $1, $3); }
  | expr T_EQ expr
    { $$ = AstOperator(T_EQ, $1, $3); }
  | expr T_MODULUS expr
    { $$ = AstOperator(T_MODULUS, $1, $3); }
  | expr T_HIGHMULT expr
    { $$ = AstOperator(T_HIGHMULT, $1, $3); current->needsHighmult = 1; }
  | expr T_LIMITMIN expr
    { $$ = AstOperator(T_LIMITMIN, $1, $3); current->needsMinMax = 1; }
  | expr T_LIMITMAX expr
    { $$ = AstOperator(T_LIMITMAX, $1, $3); current->needsMinMax = 1;}
  | expr T_REV expr
    { $$ = AstOperator(T_REV, $1, $3); }
  | expr T_ROTL expr
    { $$ = AstOperator(T_ROTL, $1, $3); current->needsRotate = 1; }
  | expr T_ROTR expr
    { $$ = AstOperator(T_ROTR, $1, $3); current->needsRotate = 1; }
  | expr T_SHL expr
    { $$ = AstOperator(T_SHL, $1, $3); }
  | expr T_SHR expr
    { $$ = AstOperator(T_SHR, $1, $3); current->needsShr = 1; }
  | expr T_SAR expr
    { $$ = AstOperator(T_SAR, $1, $3); }
  | expr T_OR expr
    { $$ = AstOperator(T_OR, $1, $3); }
  | expr T_AND expr
    { $$ = AstOperator(T_AND, $1, $3); }
  | expr '+' '=' expr %prec T_ASSIGN
    { $$ = AstAssign('+', $1, $4); }
  | expr '-' '=' expr %prec T_ASSIGN
    { $$ = AstAssign('-', $1, $4); }
  | expr '/' '=' expr %prec T_ASSIGN
    { $$ = AstAssign('/', $1, $4); }
  | expr '*' '=' expr %prec T_ASSIGN
    { $$ = AstAssign('*', $1, $4); }
  | expr '&' '=' expr %prec T_ASSIGN
    { $$ = AstAssign('&', $1, $4); }
  | expr '|' '=' expr %prec T_ASSIGN
    { $$ = AstAssign('|', $1, $4); }
  | expr '^' '=' expr %prec T_ASSIGN
    { $$ = AstAssign('^', $1, $4); }
  | expr T_MODULUS '=' expr %prec T_ASSIGN
    { $$ = AstAssign(T_MODULUS, $1, $4); }
  | expr T_HIGHMULT '=' expr %prec T_ASSIGN
    { $$ = AstAssign(T_HIGHMULT, $1, $4); current->needsHighmult = 1;}
  | expr T_LIMITMIN '=' expr %prec T_ASSIGN
    { $$ = AstAssign(T_LIMITMIN, $1, $4); current->needsMinMax = 1; }
  | expr T_LIMITMAX '=' expr %prec T_ASSIGN
    { $$ = AstAssign(T_LIMITMAX, $1, $4); current->needsMinMax = 1; }
  | expr T_REV '=' expr %prec T_ASSIGN
    { $$ = AstAssign(T_REV, $1, $4); }
  | expr T_ROTL '=' expr %prec T_ASSIGN
    { $$ = AstAssign(T_ROTL, $1, $4); current->needsRotate = 1; }
  | expr T_ROTR '=' expr %prec T_ASSIGN
    { $$ = AstAssign(T_ROTR, $1, $4); current->needsRotate = 1; }
  | expr T_SHL '=' expr %prec T_ASSIGN
    { $$ = AstAssign(T_SHL, $1, $4); }
  | expr T_SHR '=' expr %prec T_ASSIGN
    { $$ = AstAssign(T_SHR, $1, $4); current->needsShr = 1; }
  | expr T_SAR '=' expr %prec T_ASSIGN
    { $$ = AstAssign(T_SAR, $1, $4); }
  | expr T_AND '=' expr %prec T_ASSIGN
    { $$ = AstAssign(T_AND, $1, $4); }
  | expr T_OR '=' expr %prec T_ASSIGN
    { $$ = AstAssign(T_OR, $1, $4); }
  | expr '<' '=' expr %prec T_ASSIGN
    { $$ = AstAssign('<', $1, $4); }
  | expr '>' '=' expr %prec T_ASSIGN
    { $$ = AstAssign('>', $1, $4); }
  | expr T_LE '=' expr %prec T_ASSIGN
    { $$ = AstAssign(T_LE, $1, $4); }
  | expr T_GE '=' expr %prec T_ASSIGN
    { $$ = AstAssign(T_GE, $1, $4); }
  | '(' expr ')'
    { $$ = $2; }
  | '\\' funccall
    { $$ = AstCatch($2); }
  | '\\' identifier
    { $$ = AstCatch(NewAST(AST_FUNCCALL, $2, NULL)); }
  | funccall
    { $$ = $1; }
  | '-' expr %prec T_NEGATE
    {
        AST *op = $2;
        /* special case -x where x is a float constant */
        if (op->kind == AST_FLOAT) {
            op->d.ival ^= 0x80000000U;
            $$ = op;
        } else {
            $$ = AstOperator(T_NEGATE, NULL, $2);
        }
    }
  | '!' expr %prec T_BIT_NOT
    { $$ = AstOperator(T_BIT_NOT, NULL, $2); }
  | '~' expr
    { AST *shf;
      shf = AstOperator(T_SHL, $2, AstInteger(24));
      $$ = AstOperator(T_SAR, shf, AstInteger(24)); 
    }
  | T_DOUBLETILDE expr
    { AST *shf;
      shf = AstOperator(T_SHL, $2, AstInteger(16));
      $$ = AstOperator(T_SAR, shf, AstInteger(16)); 
    }
  | T_NOT expr
    { $$ = AstOperator(T_NOT, NULL, $2); }
  | T_ABS expr
    { $$ = AstOperator(T_ABS, NULL, $2); current->needsStdlib = 1; }
  | T_SQRT expr
    { $$ = AstOperator(T_SQRT, NULL, $2); current->needsSqrt = 1; }
  | T_DECODE expr
    { $$ = AstOperator(T_DECODE, NULL, $2); }
  | T_ENCODE expr
    { $$ = AstOperator(T_ENCODE, NULL, $2); current->needsBitEncode = 1; }
  | T_HERE
    { $$ = NewAST(AST_HERE, NULL, NULL); }
  | lhs T_INCREMENT
    { $$ = AstOperator(T_INCREMENT, $1, NULL); }
  | lhs T_DECREMENT
    { $$ = AstOperator(T_DECREMENT, $1, NULL); }
  | T_INCREMENT lhs
    { $$ = AstOperator(T_INCREMENT, NULL, $2); }
  | T_DECREMENT lhs
    { $$ = AstOperator(T_DECREMENT, NULL, $2); }
  | lhs '?'
    { $$ = AstOperator('?', $1, NULL); current->needsRand = 1; }
  | '?' lhs
    { $$ = AstOperator('?', NULL, $2); current->needsRand = 1; }
  | lhs '~'
    { $$ = NewAST(AST_POSTEFFECT, $1, NULL); $$->d.ival = '~'; }
  | lhs T_DOUBLETILDE
    { $$ = NewAST(AST_POSTEFFECT, $1, NULL); $$->d.ival = T_DOUBLETILDE; }
  | T_CONSTANT '(' expr ')'
    { $$ = NewAST(AST_CONSTANT, $3, NULL); }
  | T_FLOAT '(' expr ')'
    { $$ = NewAST(AST_TOFLOAT, $3, NULL); }
  | T_ROUND '(' expr ')'
    { $$ = NewAST(AST_ROUND, $3, NULL); }
  | T_TRUNC '(' expr ')'
    { $$ = NewAST(AST_TRUNC, $3, NULL); }
  | lookupexpr
    { $$ = $1; current->needsLookup = 1; }
  | lookdownexpr
    { $$ = $1; current->needsLookdown = 1; }
  ;

lhs: identifier
  | identifier '[' expr ']'
    { $$ = NewAST(AST_ARRAYREF, $1, $3); }
  | hwreg
  | hwreg '[' range ']'
    { $$ = NewAST(AST_RANGEREF, $1, $3); }
  | memref '[' expr ']'
    { $$ = NewAST(AST_ARRAYREF, $1, $3); }
  | memref
    { $$ = NewAST(AST_ARRAYREF, $1, AstInteger(0)); }
  ;

memref:
  T_BYTE '[' expr ']'
    { $$ = NewAST(AST_MEMREF, ast_type_byte, $3); }
  | T_WORD '[' expr ']'
    { $$ = NewAST(AST_MEMREF, ast_type_word, $3); }
  | T_LONG '[' expr ']'
    { $$ = NewAST(AST_MEMREF, ast_type_long, $3); }
  | identifier '.' T_BYTE
    { $$ = NewAST(AST_MEMREF, ast_type_byte, NewAST(AST_ADDROF, $1, NULL)); }
  | identifier '.' T_WORD
    { $$ = NewAST(AST_MEMREF, ast_type_word, NewAST(AST_ADDROF, $1, NULL)); }
  | identifier '.' T_LONG
    { $$ = NewAST(AST_MEMREF, ast_type_long, NewAST(AST_ADDROF, $1, NULL)); }
;

funccall:
  identifier '(' exprlist ')'
    { $$ = NewAST(AST_FUNCCALL, $1, $3); }
  | T_COGINIT '(' exprlist ')'
    { $$ = NewAST(AST_COGINIT, $1, $3); }
  | T_COGNEW '(' exprlist ')'
    {
        AST *elist;
        AST *negone = AstInteger(65536);
        elist = NewAST(AST_EXPRLIST, negone, NULL);
        elist = AddToList(elist, $3);
        $$ = NewAST(AST_COGINIT, elist, NULL);
    }
  | identifier '.' identifier '(' exprlist ')'
    { 
        $$ = NewAST(AST_FUNCCALL, NewAST(AST_METHODREF, $1, $3), $5);
    }
  | identifier '.' identifier
    { 
        $$ = NewAST(AST_FUNCCALL, NewAST(AST_METHODREF, $1, $3), NULL);
    }
  | identifier '[' expr ']' '.' identifier '(' exprlist ')'
    { 
        AST *arr = NewAST(AST_ARRAYREF, $1, $3);
        $$ = NewAST(AST_FUNCCALL, NewAST(AST_METHODREF, arr, $6), $8);
    }
  | identifier '[' expr ']' '.' identifier
    { 
        AST *arr = NewAST(AST_ARRAYREF, $1, $3);
        $$ = NewAST(AST_FUNCCALL, NewAST(AST_METHODREF, arr, $6), NULL);
    }
;


expritem:
  expr
   { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  | integer '[' expr ']'
   {
       $$ = NewAST(AST_EXPRLIST, NewAST(AST_ARRAYDECL, $1, $3), NULL);
   }
  | float '[' expr ']'
   {
       $$ = NewAST(AST_EXPRLIST, NewAST(AST_ARRAYDECL, $1, $3), NULL);
   }
  | '-' integer '[' expr ']'
   {
       AST *op = $2;
       op->d.ival = -op->d.ival;
       $$ = NewAST(AST_EXPRLIST, NewAST(AST_ARRAYDECL, op, $4), NULL);
   }
  | '-' float '[' expr ']'
   {
       AST *op = $2;
       op->d.ival ^= 0x80000000U;
       $$ = NewAST(AST_EXPRLIST, NewAST(AST_ARRAYDECL, op, $4), NULL);
   }
  | string '[' expr ']'
   {
       $$ = NewAST(AST_EXPRLIST, NewAST(AST_ARRAYDECL, $1, $3), NULL);
   }
  ;

exprlist:
  expritem
 | exprlist ',' expritem
   { $$ = AddToList($1, $3); }
 ;

operand:
  expr
   { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
 | '#' expr
   { $$ = AddToList(NewAST(AST_EXPRLIST, $2, NULL), AstInstrModifier(IMMEDIATE_INSTR)); }

operandlist:
   operand
   { $$ = $1; }
 | operandlist ',' operand
   { $$ = AddToList($1, $3); }
 ;

range:
  expr
    { $$ = NewAST(AST_RANGE, $1, NULL); }
  | expr T_DOTS expr
    { $$ = NewAST(AST_RANGE, $1, $3); }
  ;

integer:
  T_NUM
;

float:
  T_FLOATNUM
;

string:
  T_STRING
;

identifier:
  T_IDENTIFIER
  { $$ = $1; }
  | T_RESULT
  { $$ = NewAST(AST_RESULT, NULL, NULL); }
;

annotation:
  T_ANNOTATION
  { $$ = $1; }
;


hwreg:
  T_HWREG
  { $$ = $1; }
;

instruction:
  T_INSTR
  { $$ = $1; }
  | instrmodifier instruction
  { $$ = AddToList($2, $1); }
;
 
instrmodifier:
  T_INSTRMODIFIER
  { $$ = $1; }
;

modifierlist:
  instrmodifier
    { $$ = $1; }
  | modifierlist instrmodifier
    { $$ = AddToList($1, $2); }
  | modifierlist ',' instrmodifier
    { $$ = AddToList($1, $3); }
  ;

%%

void
yyerror(char *msg)
{
    fprintf(stderr, "%s:%d: %s\n", current->L.fileName, current->L.lineCounter, msg);
    gl_errors++;
}
