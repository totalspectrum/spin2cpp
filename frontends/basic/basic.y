/*
 * Spin compiler parser
 * Copyright (c) 2011-2018 Total Spectrum Software Inc.
 * See the file COPYING for terms of use.
 */

/* %define api.prefix {basicyy} */

%{
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdlib.h>
#include "frontends/common.h"
#include "frontends/lexer.h"
    
/* Yacc functions */
    void basicyyerror(const char *);
    int basicyylex();

    extern int gl_errors;

    extern AST *last_ast;
    extern AST *CommentedListHolder(AST *); // in spin.y
    
#define YYERROR_VERBOSE 1
#define YYSTYPE AST*

    
AST *GetIORegisterPair(const char *name1, const char *name2)
{
    Symbol *sym = FindSymbol(&basicReservedWords, name1);
    AST *reg1, *reg2;
    if (!sym) {
        SYNTAX_ERROR("Unknown ioregister %s", name1);
        return NULL;
    }
    reg1 = NewAST(AST_HWREG, NULL, NULL);
    reg2 = NULL;
    
    reg1->d.ptr = sym->val;
    if (name2 && gl_p2) {
        // for now, only use the register pairs on P2 (where it matters)
        // (FIXME? it could also come into play for P1V on FPGA...)
        sym = FindSymbol(&basicReservedWords, name2);
        if (sym) {
            reg2 = NewAST(AST_HWREG, NULL, NULL);
            reg2->d.ptr = sym->val;
        }
    }
    if (reg2) {
        reg1 = NewAST(AST_REGPAIR, reg1, reg2);
    }
    return reg1;
}

AST *GetPinRange(const char *name1, const char *name2, AST *range)
{
    AST *reg = GetIORegisterPair(name1, name2);
    AST *ast = NULL;
    if (reg) {
        ast = NewAST(AST_RANGEREF, reg, range);
    }
    return ast;
}

#define ARRAY_BASE_NAME "__array_base"

Symbol *
GetCurArrayBase(void)
{
#if 0
    // this would make option base work nicely inside scopes
    SymbolTable *symtab = currentTypes;
#else
    // this is more practical and keeps a value for option base that we
    // can use later for pointer dereferences
    SymbolTable *symtab = current ? &current->objsyms : currentTypes;
#endif    
    Symbol *sym = LookupSymbolInTable(symtab, ARRAY_BASE_NAME);
    if (!sym) {
        sym = AddSymbol(symtab, ARRAY_BASE_NAME, SYM_CONSTANT, AstInteger(0));
    }
    return sym;
}
        
static AST *
HandleBASICOption(AST *optid, AST *exprlist)
{
    const char *name = GetIdentifierName(optid);
    AST *expr;
    if (!name) {
        SYNTAX_ERROR("Error in option list");
        return NULL;
    }
    if (!strcmp(name, "base")) {
        int arrayBase;
        Symbol *sym;
        expr = NULL;
        if (exprlist && exprlist->kind == AST_EXPRLIST) {
            expr = exprlist->left;
            exprlist = exprlist->right;
            if (exprlist) expr = NULL;
        }
        if (!expr || !IsConstExpr(expr)) {
            SYNTAX_ERROR("option base must be followed by an integer");
            return NULL;
        }
        arrayBase = EvalConstExpr(expr);
        sym = GetCurArrayBase();
        sym->val = AstInteger(arrayBase);
    } else {
        SYNTAX_ERROR("Unknown option %s", name);
    }
    return NULL;
}

void
DeclareBASICMemberVariables(AST *ast)
{
    AST *idlist, *typ;
    AST *ident;

    if (!ast) return;
    if (ast->kind == AST_SEQUENCE) {
        ERROR(ast, "Internal error, unexpected sequence");
        return;
    }
    if (ast->kind == AST_DECLARE_ALIAS) {
        AST *name, *def;
        name = ast->left;
        def = ast->right;
        AddSymbol(&current->objsyms, name->d.string, SYM_ALIAS, (void *)def->d.string);
        return;
    }
    idlist = ast->right;
    typ = ast->left;
    if (idlist->kind == AST_LISTHOLDER) {
        while (idlist) {
            ident = idlist->left;
            MaybeDeclareMemberVar(current, ident, typ);
            idlist = idlist->right;
        }
    } else {
        MaybeDeclareMemberVar(current, idlist, typ);
    }
    return;
}

void
DeclareBASICGlobalVariables(AST *ast)
{
    AST *idlist, *typ;
    AST *ident;

    if (!ast) return;
    if (ast->kind == AST_SEQUENCE) {
        ERROR(ast, "Internal error, unexpected sequence");
        return;
    }
    if (ast->kind == AST_DECLARE_ALIAS) {
        AST *name, *def;
        name = ast->left;
        def = ast->right;
        AddSymbol(&current->objsyms, name->d.string, SYM_ALIAS, (void *)def->d.string);
        return;
    }
    idlist = ast->right;
    typ = ast->left;
    if (!idlist) {
        return;
    }
    if (typ && typ->kind == AST_EXTERN) {
        return;
    }
    if (idlist->kind == AST_LISTHOLDER) {
        while (idlist) {
            ident = idlist->left;
            DeclareOneGlobalVar(current, ident, typ);
            idlist = idlist->right;
        }
    } else {
        DeclareOneGlobalVar(current, idlist, typ);
    }
    return;
}

AST *BASICArrayRef(AST *id, AST *expr)
{
    AST *ast;
    ast = NewAST(AST_ARRAYREF, id, expr);
    return ast;
}

AST *AstCharItem(int c)
{
    AST *expr = NewAST(AST_CHAR, AstInteger(c), NULL);
    return NewAST(AST_EXPRLIST, expr, NULL);
}

#define MAX_LOOP_NEST 256
static int loop_stack[MAX_LOOP_NEST];
static int loop_sp;

static void
PushLoop(int token)
{
    if (loop_sp >= MAX_LOOP_NEST) {
        SYNTAX_ERROR("loops nested too deeply");
    } else {
        loop_stack[loop_sp++] = token;
    }
}

static void
PopLoop(void)
{
    if (loop_sp > 0) {
        --loop_sp;
    }
}

static int
GetCurrentLoop(int token)
{
    if (loop_sp == 0 || (token && loop_stack[loop_sp-1] != token)) {
        return 0;
    }
    return 1;
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
%token BAS_INSTR      "asm instruction"
%token BAS_INSTRMODIFIER "asm instruction modifier"
%token BAS_ALIGNL     "alignl"
%token BAS_ALIGNW     "alignw"

/* keywords */
%token BAS_ABS        "abs"
%token BAS_AND        "and"
%token BAS_ANY        "any"
%token BAS_AS         "as"
%token BAS_ASC        "asc"
%token BAS_ASM        "asm"
%token BAS_BYTE       "byte"
%token BAS_CATCH      "catch"
%token BAS_CASE       "case"
%token BAS_CLASS      "class"
%token BAS_CLOSE      "close"
%token BAS_CONST      "const"
%token BAS_CONTINUE   "continue"
%token BAS_CPU        "cpu"
%token BAS_DATA       "data"
%token BAS_DECLARE    "declare"
%token BAS_DELETE     "delete"
%token BAS_DIM        "dim"
%token BAS_DIRECTION  "direction"
%token BAS_DO         "do"
%token BAS_DOUBLE     "double"
%token BAS_ELSE       "else"
%token BAS_END        "end"
%token BAS_ENDIF      "endif"
%token BAS_ENUM       "enum"
%token BAS_EXIT       "exit"
%token BAS_FOR        "for"
%token BAS_FUNCTION   "function"
%token BAS_GET        "get"
%token BAS_GOTO       "goto"
%token BAS_IF         "if"
%token BAS_INPUT      "input"
%token BAS_INTEGER_KW "integer"
%token BAS_LET        "let"
%token BAS_LONG       "long"
%token BAS_LOOP       "loop"
%token BAS_MOD        "mod"
%token BAS_NEW        "new"
%token BAS_NEXT       "next"
%token BAS_NIL        "nil"
%token BAS_NOT        "not"
%token BAS_OPEN       "open"
%token BAS_OPTION     "option"
%token BAS_OR         "or"
%token BAS_OUTPUT     "output"
%token BAS_POINTER    "pointer"
%token BAS_PRINT      "print"
%token BAS_PROGRAM    "program"
%token BAS_PTR        "ptr"
%token BAS_PUT        "put"
%token BAS_READ       "read"
%token BAS_RESTORE    "restore"
%token BAS_RETURN     "return"
%token BAS_SELECT     "select"
%token BAS_SELF       "self"
%token BAS_SHARED     "shared"
%token BAS_SHORT      "short"
%token BAS_SINGLE     "single"
%token BAS_SQRT       "sqrt"
%token BAS_STEP       "step"
%token BAS_STRING_KW  "string"
%token BAS_STRUCT     "struct"
%token BAS_SUB        "sub"
%token BAS_THEN       "then"
%token BAS_THROW      "throw"
%token BAS_TO         "to"
%token BAS_TRY        "try"
%token BAS_TYPE       "type"
%token BAS_UBYTE      "ubyte"
%token BAS_UINTEGER   "uinteger"
%token BAS_ULONG      "ulong"
%token BAS_UNTIL      "until"
%token BAS_USHORT     "ushort"
%token BAS_USING      "using"
%token BAS_VAR        "var"
%token BAS_WEND       "wend"
%token BAS_WITH       "with"
%token BAS_WHILE      "while"
%token BAS_WORD       "word"
%token BAS_XOR        "xor"
%token BAS_LE         "<="
%token BAS_GE         ">="
%token BAS_NE         "<>"
%token BAS_SHL        "<<"
%token BAS_SHR        ">>"
%token BAS_NEGATE     "-"

%left BAS_FUNCTION
%left BAS_OR BAS_XOR
%left BAS_AND
%left '<' '>' BAS_LE BAS_GE BAS_NE '='
%left '-' '+'
%left '*' '/' BAS_MOD
%left BAS_SHL BAS_SHR
%left BAS_NEGATE BAS_NOT
%left '@'
%left BAS_NEW
%left '.'

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

pinrange:
  expr
    { $$ = NewAST(AST_RANGE, $1, NULL); }
  | expr ',' expr
    { $$ = NewAST(AST_RANGE, $1, $3); }
;

nonemptystatement:
  BAS_IDENTIFIER ':'
    { $$ = NewAST(AST_LABEL, $1, NULL); }
  | BAS_IDENTIFIER '=' expr eoln
    { $$ = AstAssign($1, $3); }
  | BAS_IDENTIFIER '(' expr ')' '=' expr eoln
    { $$ = AstAssign(BASICArrayRef($1, $3), $6); }
  | BAS_OUTPUT '(' pinrange ')' '=' expr eoln
    {
        $$ = AstAssign(GetPinRange("outa", "outb", $3), $6);
    }
  | BAS_DIRECTION '(' pinrange ')' '=' expr eoln
    {
        $$ = AstAssign(GetPinRange("dira", "dirb", $3), $6);
    }
  | BAS_DIRECTION '(' pinrange ')' '=' BAS_INPUT eoln
    {
        $$ = AstAssign(GetPinRange("dira", "dirb", $3), AstInteger(0));
    }
  | BAS_DIRECTION '(' pinrange ')' '=' BAS_OUTPUT eoln
    {
        $$ = AstAssign(GetPinRange("dira", "dirb", $3), AstInteger(-1));
    }
  | BAS_LET BAS_IDENTIFIER '=' expr eoln
    { MaybeDeclareMemberVar(current, $2, InferTypeFromName($2));
      $$ = AstAssign($2, $4); }
  | BAS_VAR BAS_IDENTIFIER '=' expr eoln
    {
      AST *name = $2;
      AST *assign = AstAssign(name, $4);
      assign = NewAST(AST_LISTHOLDER, assign, NULL);
      $$ = NewAST(AST_DECLARE_VAR, NULL, assign);
    }
  | BAS_IDENTIFIER '(' optexprlist ')' eoln
    { $$ = NewAST(AST_FUNCCALL, $1, $3); }
  | BAS_IDENTIFIER '.' BAS_IDENTIFIER '(' exprlist ')' eoln
    { $$ = NewAST(AST_FUNCCALL, NewAST(AST_METHODREF, $1, $3), $5); }
  | BAS_IDENTIFIER exprlist eoln
    { $$ = NewAST(AST_FUNCCALL, $1, $2); }
  | BAS_IDENTIFIER eoln
    { $$ = NewAST(AST_FUNCCALL, $1, NULL); }
  | BAS_RETURN eoln
    { $$ = AstReturn(NULL, $1); }
  | BAS_RETURN expr eoln
    { $$ = AstReturn($2, $1); }
  | BAS_DELETE expr eoln
    { $$ = NewAST(AST_DELETE, $2, NULL); }
  | BAS_GOTO BAS_IDENTIFIER eoln
    { $$ = NewAST(AST_GOTO, $2, NULL); }
  | BAS_PRINT printlist
    { $$ = NewAST(AST_PRINT, $2, NULL); }
  | BAS_PRINT '#' expr
    { $$ = NewAST(AST_PRINT, AstCharItem('\n'), $3); }
  | BAS_PRINT '#' expr ',' printlist
    { $$ = NewCommentedAST(AST_PRINT, $5, $3, $1); }
  | BAS_PUT expr eoln
    { $$ = NewCommentedAST(AST_PRINT,
                  NewAST(AST_EXPRLIST, NewAST(AST_HERE, $2, NULL), NULL),
                           NULL, $1); }
  | BAS_OPEN expr BAS_AS '#' expr
    {
        $$ = NewCommentedAST(AST_FUNCCALL,
                AstIdentifier("_basic_open"),
                NewAST(AST_EXPRLIST, $5,
                       NewAST(AST_EXPRLIST, $2, NULL)), $1);
    }
  | BAS_CLOSE '#' expr
    {
        $$ = NewCommentedAST(AST_FUNCCALL,
                    AstIdentifier("_basic_close"),
                             NewAST(AST_EXPRLIST, $3, NULL), $1);
    }
  | BAS_THROW expr
    {
        $$ = NewCommentedAST(AST_THROW, $2, NULL, $1);
    }
  | ifstmt
    { $$ = $1; }
  | whilestmt
    { $$ = $1; }
  | doloopstmt
    { $$ = $1; }
  | forstmt
    { $$ = $1; }
  | asmstmt
    { $$ = $1; }
  | trycatchstmt
    { $$ = $1; }
  | selectstmt
    { $$ = $1; }
  | exitstmt
    { $$ = $1; }
;

statement:
    nonemptystatement
    { $$ = $1; }
  | eoln
    { $$ = NULL; }
;

printitem:
  expr
    { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  | '\\' expr
    { $$ = NewAST(AST_EXPRLIST, NewAST(AST_CHAR, $2, NULL), NULL); }
;

rawprintlist:
  | printitem
  { $$ = $1; }
  | rawprintlist ';' printitem
  { $$ = AddToList($1, $3); }
  | rawprintlist ',' printitem
  { $$ = AddToList(AddToList($1, AstCharItem('\t')), $3); }
;

usingprintlist:
  | printitem
  { $$ = $1; }
  | usingprintlist ';' printitem
  { $$ = AddToList($1, $3); }
  | usingprintlist ',' printitem
  { $$ = AddToList($1, $3); }
;

printlist:
  eoln
    { $$ = AstCharItem('\n'); }
  | rawprintlist eoln
    { $$ = AddToList($1, AstCharItem('\n')); }
  | rawprintlist ',' eoln
    { $$ = AddToList($1, AstCharItem('\t')); }
  | rawprintlist ';' eoln
    { $$ = $1; }
  | BAS_USING BAS_STRING ';' usingprintlist eoln
    { $$ = NewAST(AST_USING, $2, AddToList($4, AstCharItem('\n'))); }
  | BAS_USING BAS_STRING ';' usingprintlist ';' eoln
    { $$ = NewAST(AST_USING, $2, $4); }
;

ifstmt:
  BAS_IF expr BAS_THEN eoln thenelseblock
    { $$ = NewCommentedAST(AST_IF, $2, $5, $1); }
  | BAS_IF expr nonemptystatement
    {
        AST *stmtlist = NewCommentedStatement($3);
        AST *elseblock = NewAST(AST_THENELSE, stmtlist, NULL);
        $$ = NewCommentedAST(AST_IF, $2, elseblock, $1);
    }
;

thenelseblock:
  statementlist endif
    { $$ = NewAST(AST_THENELSE, $1, NULL); }
  | statementlist BAS_ELSE eoln statementlist endif
    { $$ = NewAST(AST_THENELSE, $1, $4); }
  | statementlist BAS_ELSE nonemptystatement
    { $$ = NewAST(AST_THENELSE, $1, NewCommentedStatement($3)); }
;

endif:
  BAS_END eoln
  | BAS_END BAS_IF eoln
  | BAS_ENDIF eoln
;

whilestmt:
    BAS_WHILE expr eoln { PushLoop(BAS_WHILE); } optstatementlist endwhile
    { AST *body = CheckYield($5);
      $$ = NewCommentedAST(AST_WHILE, $2, body, $1);
      PopLoop();
    }
;

endwhile:
  BAS_WEND eoln
  | BAS_END BAS_WHILE eoln
  | BAS_END eoln
  ;

doloopstmt:
  BAS_DO BAS_WHILE expr eoln { PushLoop(BAS_DO); } optstatementlist BAS_LOOP eoln
    { AST *body = CheckYield($6);
      AST *cond = $3;
      $$ = NewCommentedAST(AST_WHILE, cond, body, $1);
      PopLoop();
    }
  | BAS_DO BAS_UNTIL expr eoln {PushLoop(BAS_DO); } optstatementlist BAS_LOOP eoln
    { AST *body = CheckYield($6);
      AST *cond = AstOperator(K_BOOL_NOT, NULL, $3);
      $$ = NewCommentedAST(AST_WHILE, cond, body, $1);
      PopLoop();
    }
  | BAS_DO eoln {PushLoop(BAS_DO); } optstatementlist doloopend
    {
        $$ = NewCommentedAST(AST_DOWHILE, $5, CheckYield($4), $1);
        PopLoop();
    }
  ;

doloopend:
  BAS_LOOP BAS_WHILE expr eoln
    { $$ = $3; }
  | BAS_LOOP BAS_UNTIL expr eoln
    { $$ = AstOperator(K_BOOL_NOT, NULL, $3); }
  | BAS_LOOP eoln
    { $$ = AstInteger(1); }
;

//
// AST_FOR consists of:
// (AST_FOR (initstmt) (AST_TO (condtest (AST_STEP step body))))
//
forstmt:
    BAS_FOR { PushLoop(BAS_FOR); } BAS_IDENTIFIER '=' expr BAS_TO expr optstep eoln statementlist endfor
    {
      AST *from, *to, *step;
      AST *ident = $3;
      AST *closeident = $11;
      AST *declare;
      AST *loop;
      
      /* create a WEAK definition for ident (it will not override any existing definition) */
      declare = NewAST(AST_DECLARE_VAR_WEAK, InferTypeFromName(ident), ident);
      step = NewAST(AST_STEP, $8, $10);
      to = NewAST(AST_TO, $7, step);
      from = NewAST(AST_FROM, $5, to);
      loop = NewCommentedAST(AST_COUNTREPEAT, $3, from, $1);
      // validate the "next i"
      if (closeident && !AstMatch(ident, closeident)) {
          ERRORHEADER(current->Lptr->fileName, current->Lptr->lineCounter, "error");
          fprintf(stderr, "Wrong variable in next: expected %s, saw %s\n", ident->d.string, closeident->d.string);
      }
      $$ = NewAST(AST_STMTLIST, declare,
                  NewAST(AST_STMTLIST, loop, NULL));
      PopLoop();
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

trycatchstmt:
  BAS_TRY eoln statementlist BAS_CATCH BAS_IDENTIFIER eoln statementlist endtry
  {
      AST *tryblock = $3;
      AST *catchvar = $5;
      AST *catchblock = $7;
      AST *try_if;
      AST *trytest;

      catchblock = NewAST(AST_STMTLIST,
                          AstAssign(catchvar, NewAST(AST_CATCHRESULT, NULL, NULL)),
                          catchblock);
      trytest = NewAST(AST_SETJMP, NULL, NULL);
      try_if = NewAST(AST_IF,
                      AstOperator(K_EQ, trytest, AstInteger(0)),
                      NewAST(AST_THENELSE, tryblock, catchblock));
      try_if = NewCommentedStatement(try_if);
      
      $$ = NewAST(AST_TRYENV, try_if, NULL);
  }
;

endtry:
  BAS_END BAS_TRY eoln
;

exitstmt:
  BAS_EXIT BAS_FUNCTION eoln
    { $$ = NewAST(AST_RETURN, NULL, NULL); }
  | BAS_EXIT BAS_SUB eoln
    { $$ = NewAST(AST_RETURN, NULL, NULL); }
  | BAS_EXIT BAS_FOR eoln
    {
        int curLoop = GetCurrentLoop(BAS_FOR);
        AST *modifier = NULL;
        if (!curLoop) {
            SYNTAX_ERROR("'exit for' is not inside a for loop");
        } else {
            modifier = AstInteger(curLoop);
        }
        $$ = NewAST(AST_QUIT, modifier, NULL);
    }
  | BAS_EXIT BAS_WHILE eoln
    {
        int curLoop = GetCurrentLoop(BAS_WHILE);
        AST *modifier = NULL;
        if (!curLoop) {
            SYNTAX_ERROR("'exit while' is not inside a while loop");
        } else {
            modifier = AstInteger(curLoop);
        }
        $$ = NewAST(AST_QUIT, modifier, NULL);
    }
  | BAS_EXIT BAS_DO eoln
    {
        int curLoop = GetCurrentLoop(BAS_DO);
        AST *modifier = NULL;
        if (!curLoop) {
            SYNTAX_ERROR("'exit do' is not inside a do loop");
        } else {
            modifier = AstInteger(curLoop);
        }
        $$ = NewAST(AST_QUIT, modifier, NULL);
    }
  | BAS_EXIT eoln
    {
        int curLoop = GetCurrentLoop(0);
        AST *modifier = NULL;
        if (!curLoop) {
            SYNTAX_ERROR("'exit' is not inside a loop");
        } else {
            modifier = AstInteger(curLoop);
        }
        $$ = NewAST(AST_QUIT, modifier, NULL);
    }
  | BAS_CONTINUE BAS_FOR eoln
    {
        int curLoop = GetCurrentLoop(BAS_FOR);
        AST *modifier = NULL;
        if (!curLoop) {
            SYNTAX_ERROR("'continue for' is not inside a for loop");
        } else {
            modifier = AstInteger(curLoop);
        }
        $$ = NewAST(AST_CONTINUE, modifier, NULL);
    }
  | BAS_CONTINUE BAS_WHILE eoln
    {
        int curLoop = GetCurrentLoop(BAS_WHILE);
        AST *modifier = NULL;
        if (!curLoop) {
            SYNTAX_ERROR("'continue while' is not inside a while loop");
        } else {
            modifier = AstInteger(curLoop);
        }
        $$ = NewAST(AST_CONTINUE, modifier, NULL);
    }
  | BAS_CONTINUE BAS_DO eoln
    {
        int curLoop = GetCurrentLoop(BAS_DO);
        AST *modifier = NULL;
        if (!curLoop) {
            SYNTAX_ERROR("'continue do' is not inside a do loop");
        } else {
            modifier = AstInteger(curLoop);
        }
        $$ = NewAST(AST_CONTINUE, modifier, NULL);
    }
  | BAS_CONTINUE eoln
    {
        int curLoop = GetCurrentLoop(0);
        AST *modifier = NULL;
        if (!curLoop) {
            SYNTAX_ERROR("'continue' is not inside a loop");
        } else {
            modifier = AstInteger(curLoop);
        }
        $$ = NewAST(AST_CONTINUE, modifier, NULL);
    }
;

selectstmt:
  BAS_SELECT BAS_CASE expr eoln casematchlist BAS_END BAS_SELECT
    { $$ = NewCommentedAST(AST_CASE, $3, $5, $1); }
;

casematchlist:
  casematchitem
    { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
  | casematchlist casematchitem
    { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $2, NULL)); }
  ;

casematchitem:
  casematch eoln statementlist
    {
        AST *slist = $3;
        $$ = NewAST(AST_CASEITEM, $1, slist);
    }
;

casematch:
  BAS_CASE expr
    {  $$ = NewAST(AST_EXPRLIST, $2, NULL); }
  | BAS_CASE expr BAS_TO expr
    {  $$ = NewAST(AST_EXPRLIST, NewAST(AST_RANGE, $2, $4), NULL); }
  | BAS_CASE BAS_ELSE
    { $$ = NewAST(AST_OTHER, NULL, NULL); }
;

optstatementlist:
  /* nothing */
    { $$ = NULL; }
  | statementlist
    { $$ = $1; }
;

statementlist:
  statement
    { $$ = NewCommentedStatement($1); }
  | dimension
    { $$ = NewCommentedStatement($1); }
  | statementlist statement
    { $$ = AddToList($1, NewCommentedStatement($2)); }
  | statementlist dimension
    { $$ = AddToList($1, NewCommentedStatement($2)); }
  ;

paramdecl:
  /* empty */
    { $$ = NULL; }
  | paramdecl1
    { $$ = $1; }
;
paramdecl1:
  paramitem
    { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
  | paramdecl1 ',' paramitem
    { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $3, NULL)); }
  ;

paramitem:
  BAS_IDENTIFIER
    { $$ = NewAST(AST_DECLARE_VAR, InferTypeFromName($1), $1); }
  | BAS_IDENTIFIER '=' expr
    { $$ = NewAST(AST_DECLARE_VAR, InferTypeFromName($1), AstAssign($1, $3)); }
  | BAS_IDENTIFIER BAS_AS typename
    { $$ = NewAST(AST_DECLARE_VAR, $3, $1); }
  | BAS_IDENTIFIER '=' expr BAS_AS typename
    { $$ = NewAST(AST_DECLARE_VAR, $5, AstAssign($1, $3)); }
;

optexprlist:
  /* nothing */
    { $$ = NULL; }
  | exprlist
    { $$ = $1; }
;

exprlist:
  expritem
    { $$ = $1; }
 | exprlist ',' expritem
   { $$ = AddToList($1, $3); }
 ;

expritem:
  expr
   { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
;

primary_expr:
  BAS_INTEGER
    { $$ = $1; }
  | BAS_FLOAT
    { $$ = $1; }
  | BAS_IDENTIFIER
    { $$ = $1; }
  | BAS_NIL
    { $$ = AstBitValue(0); } 
  | BAS_STRING
    { $$ = NewAST(AST_STRINGPTR,
                  NewAST(AST_EXPRLIST, $1, NULL), NULL); }
  | '(' expr ')'
    { $$ = $2; }
  | BAS_NEW typename
    {
        AST *numElements;
        AST *baseType;
        baseType = $2;
        if (baseType->kind == AST_ARRAYTYPE) {
            int size = TypeSize(baseType->left);
            (void)size;
            numElements = baseType->right;
            baseType = baseType->left;
        } else {
            numElements = AstInteger(1);
        }
        baseType = NewAST(AST_PTRTYPE, baseType, NULL);
        $$ = NewAST(AST_NEW, baseType, numElements);
    }
;

postfix_expr:
  primary_expr
    { $$ = $1; }
  | postfix_expr '(' ')'
    { $$ = NewAST(AST_FUNCCALL, $1, NULL); }
  | postfix_expr '(' exprlist ')'
    { $$ = NewAST(AST_FUNCCALL, $1, $3); }
  | postfix_expr '.' BAS_IDENTIFIER
    { $$ = NewAST(AST_METHODREF, $1, $3); }
  | BAS_ABS '(' expr ')'
    { $$ = AstOperator(K_ABS, NULL, $3); }  
  | BAS_ASC '(' expr ')'
    { $$ = AstOperator(K_ASC, NULL, $3); }  
  | BAS_SQRT '(' expr ')'
    { $$ = AstOperator(K_SQRT, NULL, $3); }  
  | BAS_INPUT '(' pinrange ')'
    { $$ = GetPinRange("ina", "inb", $3); }
  | BAS_OUTPUT '(' pinrange ')'
    { $$ = GetPinRange("outa", "outb", $3); }
  | BAS_DIRECTION '(' pinrange ')'
    { $$ = GetPinRange("dira", "dirb", $3); }
  | BAS_CPU '(' exprlist ')'
    {
        AST *elist;
        AST *immval = AstInteger(0x1e); // works to cognew both P1 and P2
        elist = NewAST(AST_EXPRLIST, immval, NULL);
        elist = AddToList(elist, $3);
        $$ = NewAST(AST_COGINIT, elist, NULL);
    }
  | BAS_FUNCTION '(' paramdecl ')' expr %prec BAS_FUNCTION
  {
      AST *params = $3;
      AST *rettype = NULL;
      AST *body = $5;
      AST *functype = NewAST(AST_FUNCTYPE, rettype, params);

      body = NewCommentedStatement(AstReturn(body, NULL));

      $$ = NewAST(AST_LAMBDA, functype, body);
  }
  | BAS_FUNCTION '(' paramdecl ')' BAS_AS typename eoln funcbody
  {
      AST *params = $3;
      AST *rettype = $6;
      AST *body = $8;
      AST *functype = NewAST(AST_FUNCTYPE, rettype, params);
      $$ = NewAST(AST_LAMBDA, functype, body);
  }
  | BAS_SUB '(' paramdecl ')' eoln subbody
  {
      AST *params = $3;
      AST *rettype = ast_type_void;
      AST *body = $6;
      AST *functype = NewAST(AST_FUNCTYPE, rettype, params);
      $$ = NewAST(AST_LAMBDA, functype, body);
  }
;

unary_expr:
  postfix_expr
    { $$ = $1; }
  | '+' unary_expr
    { $$ = $2; }
  | '-' unary_expr
    { $$ = AstOperator(K_NEGATE, NULL, $2); }
  | BAS_NOT unary_expr
    { $$ = AstOperator(K_BIT_NOT, NULL, $2); }
  | '@' unary_expr
    { $$ = NewAST(AST_ADDROF, $2, NULL); }
;

mult_expr:
  unary_expr
    { $$ = $1; }
  | mult_expr '*' unary_expr
    { $$ = AstOperator('*', $1, $3); }
  | mult_expr '/' unary_expr
    { $$ = AstOperator('/', $1, $3); }
  | mult_expr BAS_MOD unary_expr
    { $$ = AstOperator(K_MODULUS, $1, $3); }
  | mult_expr BAS_SHL unary_expr
    { $$ = AstOperator(K_SHL, $1, $3); }
  | mult_expr BAS_SHR unary_expr
    { $$ = AstOperator(K_SAR, $1, $3); }
;

add_expr:
  mult_expr
    { $$ = $1; }
  | add_expr '+' mult_expr  
    { $$ = AstOperator('+', $1, $3); }
  | add_expr '-' mult_expr  
    { $$ = AstOperator('-', $1, $3); }
;

compare_expr:
  add_expr
    { $$ = $1; }
  | add_expr '=' add_expr
    { $$ = AstOperator(K_EQ, $1, $3); }
  | add_expr '<' add_expr
    { $$ = AstOperator('<', $1, $3); }
  | add_expr '>' add_expr
    { $$ = AstOperator('>', $1, $3); }
  | add_expr BAS_NE add_expr
    { $$ = AstOperator(K_EQ, $1, $3); }
  | add_expr BAS_LE add_expr
    { $$ = AstOperator(K_LE, $1, $3); }
  | add_expr BAS_GE add_expr
    { $$ = AstOperator(K_GE, $1, $3); }
;

bit_expr:
  compare_expr
    { $$ = $1; }
  | bit_expr BAS_AND compare_expr
    { $$ = AstOperator('&', $1, $3); }
  | bit_expr BAS_OR compare_expr
    { $$ = AstOperator('|', $1, $3); }
  | bit_expr BAS_XOR compare_expr
    { $$ = AstOperator('^', $1, $3); }
;

expr:
  bit_expr
    { $$ = $1; }
;

topdecl:
  subdecl
  | funcdecl
  | classdecl
  | dimension
    {
        AST *ast = $1;
        if (ast->kind == AST_GLOBALVARS) {
            DeclareBASICGlobalVariables(ast->left);
        } else {
            DeclareBASICMemberVariables(ast);
        }
    }
  | constdecl
  | typedecl
  | BAS_OPTION BAS_IDENTIFIER optexprlist eoln
      {
          $$ = HandleBASICOption($2, $3);
      }
  ;

constdecl:
  BAS_CONST constlist
  {
      $$ = current->conblock = AddToList(current->conblock, $2);
  }
;
constitem:
  BAS_IDENTIFIER '=' expr
    {
      AST *decl = AstAssign($1, $3);
      decl = CommentedListHolder(decl);
      $$ = decl;
    }
;
constlist:
  constlist ',' constitem
    { $$ = AddToList($1, $3); }
  | constitem
    { $$ = $1; }
;
typedecl:
  BAS_TYPE BAS_IDENTIFIER BAS_AS typename eoln
    {
        AddSymbol(currentTypes, $2->d.string, SYM_TYPEDEF, $4);
    }
;

subdecl:
  BAS_SUB BAS_IDENTIFIER '(' paramdecl ')' eoln subbody  eoln
  {
    AST *funcdecl = NewAST(AST_FUNCDECL, $2, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $4, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    DeclareFunction(current, ast_type_void, 1, funcdef, $7, NULL, $1);
  }
  | BAS_SUB BAS_IDENTIFIER paramdecl eoln subbody eoln
  {
    AST *funcdecl = NewAST(AST_FUNCDECL, $2, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $3, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    DeclareFunction(current, ast_type_void, 1, funcdef, $5, NULL, $1);
  }
  ;

funcdecl:
  BAS_FUNCTION BAS_IDENTIFIER '(' paramdecl ')' eoln funcbody eoln
  {
    AST *name = $2;
    AST *funcdecl = NewAST(AST_FUNCDECL, name, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $4, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    AST *rettype = InferTypeFromName(name);
    DeclareFunction(current, rettype, 1, funcdef, $7, NULL, $1);
  }
  | BAS_FUNCTION BAS_IDENTIFIER '(' paramdecl ')' BAS_AS typename eoln funcbody eoln
  {
    AST *name = $2;
    AST *funcdecl = NewAST(AST_FUNCDECL, name, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $4, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    AST *rettype = $7;
    DeclareFunction(current, rettype, 1, funcdef, $9, NULL, $1);
  }
  ;

subbody:
  statementlist endsub
  { $$ = $1; }
  ;

endsub:
  BAS_END
  | BAS_END BAS_SUB
;

funcbody:
  statementlist endfunc
  { $$ = $1; }
  ;

endfunc:
  BAS_END
  | BAS_END BAS_FUNCTION
;

classdecl:
  BAS_CLASS BAS_IDENTIFIER BAS_USING BAS_STRING eoln
    {
        AST *newobj = NewAbstractObject( $2, $4 );
        current->objblock = AddToList(current->objblock, newobj);
        AddSymbol(currentTypes, $2->d.string, SYM_TYPEDEF, newobj);
        $$ = NULL;
    }
  ;

dimension:
  BAS_DIM dimlist
    { $$ = NewAST(AST_DECLARE_VAR, NULL, $2); }
  | BAS_DIM dimlist BAS_AS typename
    { $$ = NewAST(AST_DECLARE_VAR, $4, $2); }
  | BAS_DIM BAS_AS typename dimlist
    { $$ = NewAST(AST_DECLARE_VAR, $3, $4); }
  | BAS_DIM BAS_SHARED dimlist
    { $$ = NewAST(AST_GLOBALVARS, NewAST(AST_DECLARE_VAR, NULL, $3), NULL); }
  | BAS_DIM BAS_SHARED dimlist BAS_AS typename
    { $$ = NewAST(AST_GLOBALVARS, NewAST(AST_DECLARE_VAR, $5, $3), NULL); }
  | BAS_DIM BAS_SHARED BAS_AS typename dimlist
    { $$ = NewAST(AST_GLOBALVARS, NewAST(AST_DECLARE_VAR, $4, $5), NULL); }
  ;

dimlist:
  dimitem
    { $$ = $1; }
  | dimlist ',' dimitem
    { $$ = AddToList($1, $3); }
;
dimitem:
  identdecl
    { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
  | identdecl '=' expr
    { $$ = NewAST(AST_LISTHOLDER, AstAssign($1, $3), NULL); }
  | identdecl '=' '{' exprlist '}'
    { $$ = NewAST(AST_LISTHOLDER, AstAssign($1, $4), NULL); }
;

identdecl:
  BAS_IDENTIFIER
    { $$ = $1; }
  | BAS_IDENTIFIER '(' expr ')'
    {
        Symbol *sym = GetCurArrayBase();
        AST *ident = $1;
        AST *base = (AST *)sym->val;
        AST *size = $3;
        AST *decl;
        size = AstOperator('-', size, AstOperator('-', base, AstInteger(1)));
//        size = AstInteger(EvalConstExpr(size));
        decl = NewAST(AST_ARRAYDECL, ident, size);
        decl->d.ptr = base;
        $$ = decl;
    }
  | BAS_IDENTIFIER '(' expr BAS_TO expr ')'
    {
        AST *ident = $1;
        AST *base = $3;
        AST *size = $5;
        AST *decl;
        size = AstOperator('-', size, AstOperator('-', base, AstInteger(1)));
//        size = AstInteger(EvalConstExpr(size));
        decl = NewAST(AST_ARRAYDECL, ident, size);
        decl->d.ptr = base;
        $$ = decl;
    }
;


typename:
  basetypename
    { $$ = $1; }
  | basetypename ptrdef
    { $$ = NewAST(AST_PTRTYPE, $1, NULL); }
  | basetypename BAS_CONST ptrdef
    { $$ = NewAST(AST_MODIFIER_CONST, NewAST(AST_PTRTYPE, $1, NULL), NULL); }
  | '(' typename ')'
    { $$ = $2; }
  | basetypename '(' expr ')'
    {
        Symbol *sym = GetCurArrayBase();
        AST *size = $3;
        AST *base = (AST *)sym->val;
        size = AstOperator('-', size, AstOperator('-', base, AstInteger(1)));
        $$ = NewAST(AST_ARRAYTYPE, $1, size);
        $$->d.ptr = (AST *)sym->val;
    }
  ;

ptrdef:
  BAS_POINTER | BAS_PTR
  ;

basetypename:
  BAS_UBYTE
    { $$ = ast_type_byte; }
  | BAS_USHORT
    { $$ = ast_type_word; }
  | BAS_LONG
    { $$ = ast_type_long; }
  | BAS_BYTE
    { $$ = ast_type_signed_byte; }
  | BAS_SHORT
    { $$ = ast_type_signed_word; }
  | BAS_ULONG
    { $$ = ast_type_unsigned_long; }
  | BAS_INTEGER_KW
    { $$ = ast_type_long; }
  | BAS_UINTEGER
    { $$ = ast_type_unsigned_long; }
  | BAS_SINGLE
    { $$ = ast_type_float; }
  | BAS_DOUBLE
    { $$ = ast_type_float; }
  | BAS_STRING_KW
    { $$ = ast_type_string; }
  | BAS_ANY
    { $$ = ast_type_generic; }
  | BAS_TYPENAME
    { $$ = $1; }
  | BAS_CONST basetypename
    { $$ = NewAST(AST_MODIFIER_CONST, $2, NULL); }
  | BAS_FUNCTION '(' paramdecl ')' BAS_AS basetypename
    { $$ = NewAST(AST_FUNCTYPE, $6, $3); }
  | BAS_SUB '(' paramdecl ')'
    { $$ = NewAST(AST_FUNCTYPE, ast_type_void, $3); }
  | BAS_CLASS BAS_USING BAS_STRING
    {
        AST *tempnam = NewAST(AST_IDENTIFIER, NULL, NULL);
        const char *name = NewTemporaryVariable("_class_");
        AST *newobj;

        tempnam->d.string = name;
        newobj = NewAbstractObject( tempnam, $3 );        
        current->objblock = AddToList(current->objblock, newobj);
        $$ = newobj;
    }
;

asmstmt:
  BAS_ASM eoln asmlist BAS_END BAS_ASM eoln
  { $$ = NewCommentedAST(AST_INLINEASM, $3, NULL, $1); }
  ;

asmlist:
  asmline
  { $$ = $1; }
  | asmlist asmline
  { $$ = AddToList($1, $2); }
  ;

asmline:
  basedatline
  | BAS_IDENTIFIER basedatline
    {   AST *linebreak;
        AST *comment = GetComments();
        AST *ast;
        ast = $1;
        if (comment && (comment->d.string || comment->kind == AST_SRCCOMMENT)) {
            linebreak = NewCommentedAST(AST_LINEBREAK, NULL, NULL, comment);
        } else {
            linebreak = NewAST(AST_LINEBREAK, NULL, NULL);
        }
        ast = AddToList(ast, $2);
        ast = AddToList(linebreak, ast);
        $$ = ast;
    }
  ;

basedatline:
  BAS_EOLN
    { $$ = NULL; }
  | error BAS_EOLN
    { $$ = NULL; }
  | BAS_BYTE BAS_EOLN
    { $$ = NewCommentedAST(AST_BYTELIST, NULL, NULL, $1); }
  | BAS_BYTE exprlist BAS_EOLN
    { $$ = NewCommentedAST(AST_BYTELIST, $2, NULL, $1); }
  | BAS_WORD BAS_EOLN
    { $$ = NewCommentedAST(AST_WORDLIST, NULL, NULL, $1); }
  | BAS_WORD exprlist BAS_EOLN
    { $$ = NewCommentedAST(AST_WORDLIST, $2, NULL, $1); }
  | BAS_LONG BAS_EOLN
    { $$ = NewCommentedAST(AST_LONGLIST, NULL, NULL, $1); }
  | BAS_LONG exprlist BAS_EOLN
    { $$ = NewCommentedAST(AST_LONGLIST, $2, NULL, $1); }
  | instruction BAS_EOLN
    { $$ = NewCommentedInstr($1); }
  | instruction operandlist BAS_EOLN
    { $$ = NewCommentedInstr(AddToList($1, $2)); }
  | instruction modifierlist BAS_EOLN
    { $$ = NewCommentedInstr(AddToList($1, $2)); }
  | instruction operandlist modifierlist BAS_EOLN
    { $$ = NewCommentedInstr(AddToList($1, AddToList($2, $3))); }
  | BAS_ALIGNL BAS_EOLN
    { $$ = NewCommentedAST(AST_ALIGN, AstInteger(4), NULL, $1); }
  | BAS_ALIGNW BAS_EOLN
    { $$ = NewCommentedAST(AST_ALIGN, AstInteger(2), NULL, $1); }
  ;

operand:
  pasmexpr
   { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
 | '#' pasmexpr
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_IMMHOLDER, $2, NULL), NULL); }
 | '#' '#' pasmexpr
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_BIGIMMHOLDER, $3, NULL), NULL); }
 | pasmexpr '[' pasmexpr ']'
   { $$ = NewAST(AST_EXPRLIST, NewAST(AST_ARRAYREF, $1, $3), NULL); }
;

pasmexpr:
  expr
    { $$ = $1; }
  | '\\' expr
    { $$ = AstCatch($2); }
;

operandlist:
   operand
   { $$ = $1; }
 | operandlist ',' operand
   { $$ = AddToList($1, $3); }
 ;

instruction:
  BAS_INSTR
  { $$ = $1; }
  | instrmodifier instruction
  { $$ = AddToList($2, $1); }
;
 
instrmodifier:
  BAS_INSTRMODIFIER
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
basicyyerror(const char *msg)
{
    extern int saved_basicyychar;
    int yychar = saved_basicyychar;
    
    ERRORHEADER(current->Lptr->fileName, current->Lptr->lineCounter, "error");

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
