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

    extern int gl_errors;
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

%token T_INSTR
%token T_INSTRMODIFIER
%token T_HWREG
%token T_ORG

%token T_REPEAT
%token T_WHILE
%token T_UNTIL
%token T_IF
%token T_IFNOT
%token T_ELSE
%token T_ELSEIF
%token T_ELSEIFNOT

/* other stuff */
%token T_RETURN
%token T_INDENT
%token T_OUTDENT
%token T_EOLN
%token T_EOF
%token T_DOTS
%token T_COGNEW
%token T_HERE

/* operators */
%right T_ASSIGN
%left '<' '>' T_GE T_LE T_NE T_EQ
%left T_LIMITMIN T_LIMITMAX
%left '-' '+'
%left '*' '/' T_MODULUS T_HIGHMULT
%left '|' '^'
%left '&'
%left T_ROTL T_ROTR T_SHL T_SHR T_SAR T_REV
%left T_NEGATE T_BIT_NOT T_ABS T_DECODE T_ENCODE
%left '@'

%%
input:
  topelement
  | topelement input
  ;

topelement:
  T_EOLN
  | T_CON conblock
  { $$ = current->conblock = AddToList(current->conblock, $2); }
  | T_DAT datblock
  { $$ = current->datblock = AddToList(current->datblock, $2); }
  | T_VAR varblock
  { $$ = current->varblock = AddToList(current->varblock, $2); }
  | T_PUB funcdef stmtlist
  { DeclareFunction(1, $2, $3); }
  | T_PRI funcdef stmtlist
  { DeclareFunction(0, $2, $3); }
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
/* nothing */
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
  { $$ = $2 }
    ;

stmtlist:
  stmt
    { if ($1)
        $$ = NewAST(AST_STMTLIST, $1, NULL); 
      else
        $$ = NULL;
    }
  | stmtlist stmt
  { if ($2)
      $$ = AddToList($1, NewAST(AST_STMTLIST, $2, NULL)); 
    else
      $$ = $1;  
  }
  ;

stmt:
   T_RETURN T_EOLN
    { $$ = NewAST(AST_RETURN, NULL, NULL); }
  |  T_RETURN expr T_EOLN
    { $$ = NewAST(AST_RETURN, $2, NULL); }
  |  ifstmt
    { $$ = $1; }
  |  stmtblock
    { $$ = $1; }
  |  expr T_EOLN
    { $$ = $1; }
  | T_EOLN
    { $$ = NULL; }
  ;

stmtblock:
  T_INDENT stmtlist T_OUTDENT
  { $$ = $2; }
;

ifstmt:
  T_IF expr T_EOLN stmtblock
    { $$ = NewAST(AST_IF, $2, NewAST(AST_THENELSE, $4, NULL)); }
  | T_IF expr T_EOLN stmtblock T_ELSE T_EOLN stmtblock
    { $$ = NewAST(AST_IF, $2, NewAST(AST_THENELSE, $4, $7)); }
  ;


 
conblock:
  conline
  { $$ = $1; }
  | conblock conline
  { $$ = AddToList($1, $2); }
  ;

conline:
  identifier '=' expr T_EOLN
    { $$ = NewAST(AST_LISTHOLDER, NewAST(AST_ASSIGN, $1, $3), NULL); }
  | enumlist T_EOLN
    { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
  | T_EOLN
    { $$ = NULL; }
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
    { $$ = $1; }
  | datblock datline
    { $$ = AddToList($1, $2); }
  ;

datline:
  basedatline
  | identifier basedatline
    { $$ = AddToList($1, $2); }
  ;

basedatline:
  T_EOLN
    { $$ = NULL; }
  | T_BYTE exprlist T_EOLN
    { $$ = NewAST(AST_BYTELIST, $2, NULL); }
  | T_WORD exprlist T_EOLN
    { $$ = NewAST(AST_WORDLIST, $2, NULL); }
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
    { $$ = NewAST(AST_ORG, $1, NULL); }
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
  | T_EOLN
    { $$ = NULL; }
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


expr:
  integer
  | lhs
  | '@' lhs
    { $$ = NewAST(AST_OPERATOR, NULL, $2); $$->d.ival = '@'; }
  | lhs T_ASSIGN expr
    { $$ = NewAST(AST_ASSIGN, $1, $3); $$->d.ival = T_ASSIGN; }
  | expr '+' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '+'; }
  | expr '-' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '-'; }
  | expr '*' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '*'; }
  | expr '/' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '/'; }
  | expr '&' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '&'; }
  | expr '|' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '|'; }
  | expr '^' expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = '^'; }
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
  | expr T_LIMITMIN expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = T_LIMITMIN; }
  | expr T_LIMITMAX expr
    { $$ = NewAST(AST_OPERATOR, $1, $3); $$->d.ival = T_LIMITMAX; }
  | '(' expr ')'
    { $$ = $2; }
  | funccall
  | '-' expr %prec T_NEGATE
    { $$ = NewAST(AST_OPERATOR, NULL, $2); $$->d.ival = T_NEGATE; }
  | '!' expr %prec T_BIT_NOT
    { $$ = NewAST(AST_OPERATOR, NULL, $2); $$->d.ival = T_BIT_NOT; }
  | T_ABS expr
    { $$ = NewAST(AST_OPERATOR, NULL, $2); $$->d.ival = T_ABS; }
  | T_DECODE expr
    { $$ = NewAST(AST_OPERATOR, NULL, $2); $$->d.ival = T_DECODE; }
  | T_ENCODE expr
    { $$ = NewAST(AST_OPERATOR, NULL, $2); $$->d.ival = T_ENCODE; }
  | T_HERE
    { $$ = NewAST(AST_HERE, NULL, NULL); }
  ;

lhs: identifier
  | identifier '[' expr ']'
    { $$ = NewAST(AST_ARRAYDECL, $1, $3); }
  | hwreg
  | hwreg '[' range ']'
    { $$ = NewAST(AST_RANGEREF, $1, $3); }
  ;

funccall:
  identifier '(' exprlist ')'
  { $$ = NewAST(AST_FUNCCALL, $1, $3); }
  | T_COGNEW '(' exprlist ')'
  { $$ = NewAST(AST_COGNEW, $1, $3); }
;

exprlist:
  expr
   { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
 | exprlist ',' expr
   { $$ = AddToList($1, NewAST(AST_EXPRLIST, $3, NULL)); }
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
    { $$ = NewAST(AST_RANGE, $1, $1); }
  | expr T_DOTS expr
    { $$ = NewAST(AST_RANGE, $1, $3); }
  ;

integer:
  T_NUM
  { $$ = current->ast; }
;

identifier:
  T_IDENTIFIER
  { $$ = current->ast; }
;

hwreg:
  T_HWREG
  { $$ = current->ast; }
;

instruction:
  T_INSTR
  { $$ = current->ast; }
  | instrmodifier instruction
  { $$ = AddToList($2, $1); }
;
 
instrmodifier:
  T_INSTRMODIFIER
  { $$ = current->ast; }
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
    fprintf(stderr, "%s at line %d\n", msg, current->L.lineCounter);
    gl_errors++;
}
