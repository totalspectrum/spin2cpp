/*
 * BASIC compiler parser
 * Copyright (c) 2011-2022 Total Spectrum Software Inc.
 * See the file COPYING for terms of use.
 */

%pure-parser

%{
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdlib.h>
#include "frontends/common.h"
#include "frontends/lexer.h"

#define BASICYYSTYPE AST*
#undef  YYSTYPE
#define YYSTYPE BASICYYSTYPE
    
/* Yacc functions */
    void basicyyerror(const char *);
    int basicyylex();

    extern int gl_errors;

    extern AST *last_ast;
    extern AST *CommentedListHolder(AST *); // in spin.y
    
#define YYERROR_VERBOSE 1

extern AST *IntegerLabel(AST *);
    
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

static AST *
InputHandle(AST *h)
{
    return h ? h : AstInteger(0);
}

static AST *
OutputHandle(AST *h)
{
    return h ? h : AstInteger(1);
}

#define ARRAY_BASE_NAME "__array_base"
#define IMPLICIT_TYPE_NAME "__implicit_types"
#define EXPLICIT_DECL_NAME "__explicit_declares_required"
/* __explicit_declares_required is a bitmask saying which kinds of things
 * require explicit declarations:
 *  1 == assignment statements without LET
 *  2 == assignment statements with LET
 *  4 == read statements
 *  8 == for loop variables
 */
#define DEFAULT_EXPLICIT_DECLARES 0x5
#define GetExplicitDeclares() GetCurOptionSymbol(EXPLICIT_DECL_NAME, DEFAULT_EXPLICIT_DECLARES)

Symbol *
GetCurOptionSymbol(const char *name, int defval)
{
#if 0
    // this would make option base work nicely inside scopes
    SymbolTable *symtab = currentTypes;
#else
    // this is more practical and keeps a value for option base that we
    // can use later for pointer dereferences
    SymbolTable *symtab = current ? &current->objsyms : currentTypes;
#endif    
    Symbol *sym = LookupSymbolInTable(symtab, name);
    if (!sym) {
        sym = AddSymbol(symtab, name, SYM_CONSTANT, AstInteger(defval), NULL);
    }
    return sym;
}

Symbol *
GetCurArrayBase(void)
{
    return GetCurOptionSymbol(ARRAY_BASE_NAME, 0);
}

Symbol *
GetCurImplicitTypes(void)
{
    return GetCurOptionSymbol(IMPLICIT_TYPE_NAME, 0);
}

static AST *
HandleBASICOption(AST *optid, AST *exprlist)
{
    const char *name = GetIdentifierName(optid);
    AST *expr;
    Symbol *sym;
    if (!name) {
        SYNTAX_ERROR("Error in option list");
        return NULL;
    }
    if (!strcasecmp(name, "base")) {
        int arrayBase;
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
    } else if (!strcasecmp(name, "explicit")) {
        sym = GetExplicitDeclares();
        sym->val = AstInteger(255);
    } else if (!strcasecmp(name, "implicit")) {
        sym = GetExplicitDeclares();
        sym->val = AstInteger(0);
    } else {
        SYNTAX_ERROR("Unknown option %s", name);
    }
    return NULL;
}

AST *
DefineDefaultVarTypes(AST *deflist, AST *type)
{
    Symbol *sym = GetCurImplicitTypes();
    AST *entry;
    const char *first, *last;
    uint32_t flags;
    uint32_t low, high;
    uint32_t mask;

    flags = EvalConstExpr( (AST *)sym->val );
    if (IsIntType(type)) {
        mask = 0;
    } else if (IsFloatType(type)) {
        mask = 1;
    } else {
        SYNTAX_ERROR("defXXX is supported only for int and sng");
        return NULL;
    }
    while (deflist) {
        entry = deflist->left;
        deflist = deflist->right;
        if (entry->kind != AST_SEQUENCE) {
            SYNTAX_ERROR("Internal error, expected AST_SEQUENCE");
            return NULL;
        }
        if (!entry->left || !entry->right) {
            SYNTAX_ERROR("Internal error in defvar parsing");
            return NULL;
        }
        first = GetIdentifierName(entry->left);
        last = GetIdentifierName(entry->right);
        low = toupper(first[0]) - 'A';
        high = toupper(last[0]) - 'A';
        if (strlen(first) != 1 || strlen(last) != 1 || low > 25 || high > 25) {
            SYNTAX_ERROR("defXXX requires single letters in patterns");
            return NULL;
        }
        if (high < low) {
            WARNING(NULL, "range `%c' - `%c' is empty", low, high);
        }
        while (low <= high) {
            if (mask) {
                flags |= (1U << low);
            } else {
                flags &= ~(1U << low);
            }
            low++;
        }
    }
    sym->val = AstInteger(flags);
    return NULL;
}

void
DeclareBASICMemberVariables(AST *ast)
{
    AST *idlist, *typ;
    AST *ident;
    int is_private = 0;
    
    if (!ast) return;
    if (ast->kind == AST_SEQUENCE) {
        ERROR(ast, "Internal error, unexpected sequence");
        return;
    }
    if (ast->kind == AST_DECLARE_ALIAS) {
        AST *newname = ast->left;
        AST *oldname = ast->right;
        Symbol *sym;
        if (!newname || !oldname) {
            ERROR(ast, "Internal error, bad alias structure");
            return;
        }
        sym = DeclareAlias(&current->objsyms, newname, oldname);
        (void)sym;
        return;
    }
    if (ast->kind != AST_DECLARE_VAR) {
        ERROR(ast, "Internal error, unexpected ast");
        return;
    }
    idlist = ast->right;
    typ = ast->left;
    if (idlist->kind == AST_LISTHOLDER) {
        while (idlist) {
            ident = idlist->left;
            MaybeDeclareMemberVar(current, ident, typ, is_private, NORMAL_VAR);
            idlist = idlist->right;
        }
    } else {
        MaybeDeclareMemberVar(current, idlist, typ, is_private, NORMAL_VAR);
    }
    return;
}

void
DeclareBASICSharedVariables(AST *ast)
{
    // declare globals in DAT section
    DeclareTypedGlobalVariables(ast, 1);
}

void
DeclareBASICRegisterVariables(AST *ast)
{
    // declare globals in DAT section
    DeclareTypedRegisterVariables(ast);
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

AST *AddTemplateTypes(AST *list)
{
  AST *orig = list;
  AST *ident;
  while (list) {
    ident = list->left;
    list = list->right;
    AddSymbol(currentTypes, ident->d.string, SYM_TYPEDEF, (void *)ident, NULL);
  }
  return orig;
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

static AST *
BuildOnGotoCases(AST *exprlist)
{
    AST *list = NULL;
    AST *target;
    AST *item;
    int index = 1;
    AST *gostmt;
    while (exprlist) {
        target = exprlist->left;
        exprlist = exprlist->right;
        if (IsIdentifier(target)) {
            gostmt = NewAST(AST_GOTO, target, NULL);
        } else if (target->kind == AST_INTEGER) {
            gostmt = NewAST(AST_GOTO, IntegerLabel(target), NULL);
        } else {
            SYNTAX_ERROR("ON GOTO accepts labels or integers only");
            return NULL;
        }
        gostmt = NewAST(AST_STMTLIST, gostmt, NULL);
        item = NewAST(AST_EXPRLIST, AstInteger(index), NULL);
        item = NewAST(AST_CASEITEM, item, gostmt);
        item = NewAST(AST_STMTLIST, item, NULL);
        list = AddToList(list, item);
        index++;
    }
    return list;
}

static AST *
AdjustParamType(AST *param)
{
    AST *typ = param->left;
    if (typ && !IsIdentifier(typ) && (IsClassType(typ) || IsArrayType(typ))) {
        param->left = NewAST(AST_REFTYPE, typ, NULL);
    }
    return param;        
}

static AST *
AdjustParamForByRef(AST *param)
{
    AST *typ = param->left;
    param->left = NewAST(AST_REFTYPE, typ, NULL);
    return param;
}

static AST *
AdjustParamForByVal(AST *param)
{
    AST *typ = param->left;
    if (typ && !IsIdentifier(typ) && (IsClassType(typ) || IsArrayType(typ))) {
        param->left = NewAST(AST_COPYREFTYPE, typ, NULL);
    }
    return param;
}

%}

%token BAS_EMPTY      "_"
%token BAS_IDENTIFIER "identifier"
%token BAS_LABEL_INFERRED      "label"
%token BAS_LABEL_EXPLICIT       "label:"
%token BAS_INTEGER    "integer number"
%token BAS_FLOAT      "number"
%token BAS_STRING     "literal string"
%token BAS_EOLN       "end of line"
%token BAS_EOF        "end of file"
%token BAS_TYPENAME   "type name"
%token BAS_INSTR      "asm instruction"
%token BAS_INSTRMODIFIER "asm instruction modifier"
%token BAS_HWREG      "hardware register"
%token BAS_ALIGNL     "alignl"
%token BAS_ALIGNW     "alignw"
%token BAS_FILE       "file"
%token BAS_FIT        "fit"
%token BAS_ORG        "org"
%token BAS_ORGF       "orgf"
%token BAS_ORGH       "orgh"
%token BAS_RES        "res"

/* keywords */
%token BAS_ABS        "abs"
%token BAS_ALIAS      "alias"
%token BAS_AND        "and"
%token BAS_ANDALSO    "andalso"
%token BAS_ANY        "any"
%token BAS_APPEND     "append"
%token BAS_AS         "as"
%token BAS_ASC        "asc"
%token BAS_ASM        "asm"
%token BAS_ALLOCA     "__builtin_alloca"
%token BAS_BOOLEAN    "boolean"
%token BAS_BYREF      "byref"
%token BAS_BYTE       "byte"
%token BAS_BYVAL      "byval"
%token BAS_CALL       "call"
%token BAS_CASE       "case"
%token BAS_CAST       "cast"
%token BAS_CATCH      "catch"
%token BAS_CHAIN      "chain"
%token BAS_CLASS      "class"
%token BAS_CLOSE      "close"
%token BAS_CONST      "const"
%token BAS_CONTINUE   "continue"
%token BAS_CPU        "cpu"
%token BAS_DATA       "data"
%token BAS_DECLARE    "declare"
%token BAS_DEF        "def"
%token BAS_DEFINT     "defint"
%token BAS_DEFSNG     "defsng"
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
%token BAS_EXTERN     "extern"
%token BAS_FIXED      "fixed"
%token BAS_FOR        "for"
%token BAS_FUNCTION   "function"
%token BAS_FUNC_NAME  "__FUNCTION__"
%token BAS_GET        "get"
%token BAS_GOTO       "goto"
%token BAS_GOSUB      "gosub"
%token BAS_HASMETHOD  "_hasmethod"
%token BAS_IF         "if"
%token BAS_IMPORT     "import"
%token BAS_INPUT      "input"
%token BAS_CAST_INT   "int"
%token BAS_INTEGER_KW "integer"
%token BAS_LET        "let"
%token BAS_LIB        "lib"
%token BAS_LINE       "line"
%token BAS_LONG       "long"
%token BAS_LONGINT    "longint"
%token BAS_LOOP       "loop"
%token BAS_MOD        "mod"
%token BAS_NEW        "new"
%token BAS_NEXT       "next"
%token BAS_NIL        "nil"
%token BAS_NOT        "not"
%token BAS_ON         "on"
%token BAS_OPEN       "open"
%token BAS_OPTION     "option"
%token BAS_OR         "or"
%token BAS_ORELSE     "orelse"
%token BAS_OUTPUT     "output"
%token BAS_POINTER    "pointer"
%token BAS_PRESERVE   "preserve"
%token BAS_PRINT      "print"
%token BAS_PRIVATE    "private"
%token BAS_PROGRAM    "program"
%token BAS_PTR        "ptr"
%token BAS_PUT        "put"
%token BAS_READ       "read"
%token BAS_REDIM      "redim"
%token BAS_REGISTER   "register"
%token BAS_RESTORE    "restore"
%token BAS_RETURN     "return"
%token BAS_SAMETYPES  "_sametypes"
%token BAS_SELECT     "select"
%token BAS_SELF       "self"
%token BAS_SHARED     "shared"
%token BAS_SHORT      "short"
%token BAS_SINGLE     "single"
%token BAS_SIZEOF     "sizeof"
%token BAS_SQRT       "sqrt"
%token BAS_STEP       "step"
%token BAS_STRING_KW  "string"
%token BAS_STRUCT     "struct"
%token BAS_SUB        "sub"
%token BAS_THEN       "then"
%token BAS_THROW      "throw"
%token BAS_THROWIFCAUGHT      "throwifcaught"
%token BAS_TO         "to"
%token BAS_TRY        "try"
%token BAS_TYPE       "type"
%token BAS_UBYTE      "ubyte"
%token BAS_UINTEGER   "uinteger"
%token BAS_ULONG      "ulong"
%token BAS_ULONGINT   "ulongint"
%token BAS_UNION      "union"
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
%token BAS_SHL        "shl"
%token BAS_SHR        "shr"

%token BAS_ADD_ASSIGN "+="
%token BAS_SUB_ASSIGN "-="
%token BAS_MUL_ASSIGN "*="
%token BAS_DIV_ASSIGN "/="
%token BAS_MOD_ASSIGN "MOD="
%token BAS_AND_ASSIGN "AND="
%token BAS_OR_ASSIGN "OR="
%token BAS_XOR_ASSIGN "XOR="

%left BAS_EOLN
%left BAS_FUNCTION
%left BAS_OR BAS_XOR
%left BAS_AND
%left '<' '>' BAS_LE BAS_GE BAS_NE '='
%left '-' '+'
%left '*' '/' BAS_MOD
%left BAS_SHL BAS_SHR
%left BAS_NOT
%left '@'
%left BAS_NEW
%left '.'
%left '('

%%

toplist:
 topitem
 | toplist eoln topitem
 | toplist BAS_END BAS_EOLN
;

eoln:
  ':'
  | eolnseq
;

eolnseq:
  BAS_EOLN
  | eolnseq BAS_EOLN
  ;


toplabel: BAS_LABEL_INFERRED
  {
        AST *label = NewAST(AST_LABEL, $1, NULL);
        AST *stmt = NewAST(AST_STMTLIST, label, NULL);
        current->body = AddToList(current->body, stmt);
  }
;


topitem:
    /* empty */
    { $$ = NULL; }
  | statement
    {
        AST *stmtholder = NewAST(AST_STMTLIST, $1, NULL);
        current->body = AddToList(current->body, stmtholder);
        $$ = stmtholder;
    }
  | topdecl
    { $$ = $1; }
  | BAS_DATA
    {
        AST *list = NewAST(AST_EXPRLIST, $1, NULL);
        current->bas_data = AddToListEx(current->bas_data, list, &current->bas_data_tail);
    }
  | toplabel topitem
    {
        $$ = $2;
    }
;

wrapped_stmt:
  statement
    {
        $$ = NewAST(AST_STMTLIST, $1, NULL);
    }
  | BAS_LABEL_INFERRED wrapped_stmt
    {
        AST *label = NewAST(AST_LABEL, $1, NULL);
        $$ = NewAST(AST_STMTLIST, label, $2);
    }
  | BAS_LABEL_INFERRED BAS_EOLN wrapped_stmt
    {
        AST *label = NewAST(AST_LABEL, $1, NULL);
        $$ = NewAST(AST_STMTLIST, label, $3);
    }
;

optstatementlist:
  /* empty */
    { $$ = NULL; }
  | statementlist
    { $$ = $1; }
;

stmtlistitem:
  wrapped_stmt
    { $$ = $1; }
  | dimension
    { $$ = NewAST(AST_STMTLIST, $1, NULL); }
  | BAS_LABEL_INFERRED
    { $$ = NewAST(AST_STMTLIST, NewAST(AST_LABEL, $1, NULL), NULL); }
  | BAS_LABEL_INFERRED dimension
      {
        AST *label = NewAST(AST_STMTLIST, NewAST(AST_LABEL, $1, NULL), NULL);
        AST *dim = NewAST(AST_STMTLIST, $2, label);
        $$ = dim;
      }
;

statementlist:
  realstatementlist
    { $$ = $1; }
  | eoln statementlist
    { $$ = $2; }
;

realstatementlist:
  stmtlistitem
    { $$ = $1; }
  | stmtlistitem eoln optstatementlist
    { $$ = AddToList($1, $3); }
;

iflist:
  statement
    { $$ = NewAST(AST_STMTLIST, $1, NULL); }
  | iflist ':' statement
    { $$ = AddToList($1, NewAST(AST_STMTLIST, $3, NULL)); }
;

optelselist:
  BAS_ELSE iflist
     { $$ = $2; }
  | /* nothing */
    { $$ = NULL; }
;

ifline:
  iflist optelselist
     { $$ = NewAST(AST_THENELSE, $1, $2); }
;

statement:
  assign_statement
    {
        $$ = $1;
    }
  | BAS_IDENTIFIER np_exprlist /* np means "no parentheses" */
    {
        AST *params;
        params = $2;
        $$ = NewAST(AST_FUNCCALL, $1, params);
    }
  | BAS_CALL BAS_IDENTIFIER
    {
        AST *params;
        params = NULL;
        $$ = NewAST(AST_FUNCCALL, $2, params);
    }  
  | BAS_CALL BAS_IDENTIFIER '(' ')'
    {
        AST *params;
        params = NULL;
        $$ = NewAST(AST_FUNCCALL, $2, params);
    }  
  | BAS_CALL BAS_IDENTIFIER '(' exprlist ')'
    {
        AST *params;
        params = $4;
        $$ = NewAST(AST_FUNCCALL, $2, params);
    }
  | BAS_VAR BAS_IDENTIFIER '=' expr
    {
      AST *name = $2;
      AST *assign = AstAssign(name, $4);
      assign = NewAST(AST_LISTHOLDER, assign, NULL);
      $$ = NewAST(AST_DECLARE_VAR, NULL, assign);
    }
  | varexpr
    {
        AST *top = $1;
        AST *ast = top;
        while (ast && ast->kind == AST_COMMENTEDNODE) {
            ast = ast->left;
        }
        if (ast && ast->kind != AST_FUNCCALL) {
            $$ = NewAST(AST_FUNCCALL, ast, NULL);
        } else {
            $$ = top;
        }
    }
  | BAS_DELETE expr
    { $$ = NewAST(AST_DELETE, $2, NULL); }
  | iostmt
    { $$ = $1; }
  | branchstmt
    { $$ = $1; }
  | ifstmt
    { $$ = $1; }
  | whilestmt
    { $$ = $1; }
  | doloopstmt
    { $$ = $1; }
  | forstmt
    { $$ = $1; }
  | trycatchstmt
    { $$ = $1; }
  | selectstmt
    { $$ = $1; }
  | asmstmt
    { $$ = $1; }
;

assignment_operator
	: '='
            { $$ = AstAssign(NULL, NULL); }
	| BAS_MUL_ASSIGN
            { $$ = AstOpAssign('*', NULL, NULL); }
	| BAS_DIV_ASSIGN
            { $$ = AstOpAssign('/', NULL, NULL); }
	| BAS_ADD_ASSIGN
            { $$ = AstOpAssign('+', NULL, NULL); }
	| BAS_SUB_ASSIGN
            { $$ = AstOpAssign('-', NULL, NULL); }
	| BAS_AND_ASSIGN
            { $$ = AstOpAssign('&', NULL, NULL); }
	| BAS_XOR_ASSIGN
            { $$ = AstOpAssign('^', NULL, NULL); }
	| BAS_OR_ASSIGN
            { $$ = AstOpAssign('|', NULL, NULL); }
	;


varassignlist:
  varassigntarget ',' multivars
      { $$ = NewAST(AST_EXPRLIST, $1, $3); }
  ;

multivars:
  varassigntarget
      { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  | multivars ',' varassigntarget
      { $$ = AddToList($1, NewAST(AST_EXPRLIST, $3, NULL)); }
  ;


simple_assign_statement:
  varexpr assignment_operator expr
    {
        AST *op = $2;
        op->left = $1;
        op->right = $3;
        $$ = op;
    }
  | varassignlist '=' exprlist
  {
      $$ = AstAssignList($1, $3, $2);
  }
  | register_expr '=' expr
    {
        AST *op = AstAssign(NULL, NULL);
        op->left = $1;
        op->right = $3;
        $$ = op;
    }
  | register_expr '=' BAS_INPUT
    { $$ = AstAssign($1, AstInteger(0)); }
  | register_expr '=' BAS_OUTPUT
    { $$ = AstAssign($1, AstInteger(-1)); }
;
assign_statement:
  simple_assign_statement
    {
        AST *assign = $1;
        AST *ident;
        Symbol *sym = GetExplicitDeclares();
        int explicit_flag = EvalConstExpr((AST *)sym->val);
        if (0 == (explicit_flag & 0x1) ) {
            ident = assign->left;
            MaybeDeclareMemberVar(current, ident, NULL, 0, IMPLICIT_VAR);
        }
        $$ = assign;
    }
  | BAS_LET simple_assign_statement
    {
        AST *assign = $2;
        AST *ident;
        Symbol *sym = GetExplicitDeclares();
        int explicit_flag = EvalConstExpr((AST *)sym->val);
        if (0 == (explicit_flag & 0x2) ) {
            ident = assign->left;
            MaybeDeclareMemberVar(current, ident, NULL, 0, IMPLICIT_VAR);
        }
        $$ = assign;
    }
;

branchstmt:
  exitstmt
  | BAS_RETURN
    { $$ = AstReturn(NULL, $1); }
  | BAS_RETURN exprlist
    { $$ = AstReturn($2, $1); }
  | BAS_LABEL_EXPLICIT
    { $$ = NewAST(AST_LABEL, $1, NULL); }
  | BAS_GOTO BAS_IDENTIFIER
    { $$ = NewAST(AST_GOTO, $2, NULL); }
  | BAS_GOTO BAS_INTEGER
    { $$ = NewAST(AST_GOTO, IntegerLabel($2), NULL); }
  | BAS_GOSUB BAS_IDENTIFIER
    { $$ = NewAST(AST_GOSUB, $2, NULL); }
  | BAS_GOSUB BAS_INTEGER
    { $$ = NewAST(AST_GOSUB, IntegerLabel($2), NULL); }
  | BAS_THROW expr
    {
        $$ = NewCommentedAST(AST_THROW, $2, NULL, $1);
    }
  | BAS_THROWIFCAUGHT expr
    {
        AST *top = NewCommentedAST(AST_THROW, $2, NULL, $1);
        AST *throwit;
        if (top && top->kind != AST_THROW) {
            throwit = top->left;
        } else {
            throwit = top;
        }
        throwit->d.ival = 1;
        $$ = top;
    }
;

optcomma: /* nothing */ | ',' ;

file_handle:
   { $$ = NULL; }
| '#' expr optcomma
   { $$ = $2; }
;

iostmt:
  BAS_PRINT file_handle printlist
    { $$ = NewCommentedAST(AST_PRINT, $3, $2, $1); }
  | BAS_PRINT file_handle ',' printlist
    { $$ = NewCommentedAST(AST_PRINT, $4, $2, $1); }
  | BAS_LINE BAS_INPUT file_handle varexpr
    {
        AST *handle = $3;
        AST *var = $4;
        AST *readit = NewAST(AST_FUNCCALL,
                             AstIdentifier("_basic_read_line"),
                             NewAST(AST_EXPRLIST, handle, NULL));
        $$ = AstAssign(var, readit);
    }
  | BAS_INPUT file_handle inputlist
    { $$ = NewCommentedAST(AST_READ, $3, InputHandle($2), $1); }
  | BAS_INPUT file_handle BAS_STRING ',' inputlist
    {
        AST *inhandle = InputHandle($2);
        AST *outhandle = OutputHandle($2);
        AST *string = NewAST(AST_STRINGPTR,
                             NewAST(AST_EXPRLIST, $3, NULL), NULL);
        AST *printlist = NewAST(AST_EXPRLIST, string, NULL);
        AST *printstmt = NewCommentedAST(AST_PRINT, printlist, outhandle, $1);
        AST *inpstmt = NewAST(AST_READ, $5, inhandle);
        AST *stmt;
        stmt = NewAST(AST_STMTLIST,
                      printstmt,
                      NewAST(AST_STMTLIST, inpstmt, NULL));
        $$ = stmt;
    }
  | BAS_INPUT file_handle BAS_STRING ';' inputlist
    {
        AST *inhandle = InputHandle($2);
        AST *outhandle = InputHandle($2);
        AST *question = NewAST(AST_EXPRLIST,
                               AstInteger(63),
                               NewAST(AST_EXPRLIST, AstInteger(32), NULL));
        AST *string = NewAST(AST_STRINGPTR,
                             NewAST(AST_EXPRLIST, $3, question), NULL);
        AST *printlist = NewAST(AST_EXPRLIST, string, NULL);
        AST *printstmt = NewCommentedAST(AST_PRINT, printlist, outhandle, $1);
        AST *inpstmt = NewAST(AST_READ, $5, inhandle);
        AST *stmt;
        stmt = NewAST(AST_STMTLIST,
                      printstmt,
                      NewAST(AST_STMTLIST, inpstmt, NULL));
        $$ = stmt;
    }
  | BAS_READ inputlist
    { $$ = NewCommentedAST(AST_READ, $2, NULL, $1); }
  | BAS_RESTORE
    {
      AST *varname = AstIdentifier("__basic_data_ptr");
      AST *labelref = AstIdentifier("__basic_data");
      AST *init;
      init = AstAssign(varname, NewAST(AST_ADDROF, labelref, NULL));
      $$ = init;
    }
  | BAS_OPEN expr BAS_AS '#' expr
    {
        $$ = NewCommentedAST(AST_FUNCCALL,
                AstIdentifier("_basic_open"),
                NewAST(AST_EXPRLIST, $5,
                       NewAST(AST_EXPRLIST, $2, NULL)), $1);
    }
  | BAS_OPEN expr BAS_FOR BAS_INPUT BAS_AS '#' expr
    {
        $$ = NewCommentedAST(AST_FUNCCALL,
                AstIdentifier("_basic_open_string"),
                NewAST(AST_EXPRLIST, $7,
                       NewAST(AST_EXPRLIST, $2,
                              /* mode 0 is O_RDONLY */
                              NewAST(AST_EXPRLIST, AstInteger(0), NULL))), $1);
    }
  | BAS_OPEN expr BAS_FOR BAS_OUTPUT BAS_AS '#' expr
    {
        $$ = NewCommentedAST(AST_FUNCCALL,
                AstIdentifier("_basic_open_string"),
                NewAST(AST_EXPRLIST, $7,
                       NewAST(AST_EXPRLIST, $2,
                              /* mode is O_WRONLY | O_CREAT | O_TRUNC */
                              NewAST(AST_EXPRLIST, AstInteger(1 + 4 + 8), NULL))), $1);
    }
  | BAS_OPEN expr BAS_FOR BAS_APPEND BAS_AS '#' expr
    {
        $$ = NewCommentedAST(AST_FUNCCALL,
                AstIdentifier("_basic_open_string"),
                NewAST(AST_EXPRLIST, $7,
                       NewAST(AST_EXPRLIST, $2,
                              /* mode is O_WRONLY | O_CREAT | O_APPEND */
                              NewAST(AST_EXPRLIST, AstInteger(1 + 4 + 32), NULL))), $1);
    }
  | BAS_CLOSE '#' expr
    {
        $$ = NewCommentedAST(AST_FUNCCALL,
                    AstIdentifier("_basic_close"),
                             NewAST(AST_EXPRLIST, $3, NULL), $1);
    }
  | BAS_CHAIN '#' BAS_INTEGER
    {
        AST *twonulls = NewAST(AST_EXPRLIST,
                              AstInteger(0),
                              NewAST(AST_EXPRLIST,
                                     AstInteger(0),
                                     NULL));
        AST *handle = $3;
        $$ = NewCommentedAST(AST_FUNCCALL,
                    AstIdentifier("_fexecve"),
                             NewAST(AST_EXPRLIST, handle, twonulls), $1);
    }
  | BAS_CHAIN BAS_STRING
    {
        AST *twonulls = NewAST(AST_EXPRLIST,
                              AstInteger(0),
                              NewAST(AST_EXPRLIST,
                                     AstInteger(0),
                                     NULL));
        AST *name = NewAST(AST_STRINGPTR, NewAST(AST_EXPRLIST, $2, NULL), NULL);
        $$ = NewCommentedAST(AST_FUNCCALL,
                    AstIdentifier("_execve"),
                             NewAST(AST_EXPRLIST, name, twonulls), $1);
    }
  | BAS_GET '#' putgetargs
    {
        AST *params = $3;
        AST *thefunc = AstIdentifier("_basic_get");
        AST *result = NULL;
        AST *call;
        AST *comment = $1;
        if (params && params->kind == AST_ASSIGN) {
            result = params->left;
            params = params->right;
        }
        call = NewAST(AST_FUNCCALL, thefunc, params);
        if (result) {
            call = AstAssign(result, call);
        }
        if (comment) {
            call = NewAST(AST_COMMENTEDNODE, call, comment);
        }
        $$ = call;
    }
  | BAS_PUT '#' putgetargs
    {
        AST *params = $3;
        AST *thefunc = AstIdentifier("_basic_put");
        AST *result = NULL;
        AST *call;
        AST *comment = $1;
        if (params && params->kind == AST_ASSIGN) {
            result = params->left;
            params = params->right;
        }
        call = NewAST(AST_FUNCCALL, thefunc, params);
        if (result) {
            call = AstAssign(result, call);
        }
        if (comment) {
            call = NewAST(AST_COMMENTEDNODE, call, comment);
        }
        $$ = call;
    }
;

inputitem:
  varexpr
    {
        Symbol *sym = GetExplicitDeclares();
        int explicit_flag = EvalConstExpr((AST *)sym->val);
        if (0 == (explicit_flag & 0x4)) {
            MaybeDeclareMemberVar(current, $1, NULL, 0, IMPLICIT_VAR);
        }
        $$ = NewAST(AST_EXPRLIST, $1, NULL);
    }
;

inputlist:
  inputitem
    { $$ = $1; }
  | inputlist ',' inputitem
    {
        $$ = AddToList($1, $3);
    }
  ;

printitem:
  expr
    { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  | '\\' expr
    { $$ = NewAST(AST_EXPRLIST, NewAST(AST_CHAR, $2, NULL), NULL); }
;

rawprintlist:
  printitem
    { $$ = $1; }
  | rawprintlist ';' printitem
    { $$ = AddToList($1, $3); }
  | rawprintlist ',' printitem
    { $$ = AddToList(AddToList($1, AstCharItem('\t')), $3); }
;

usingprintlist:
  printitem
  { $$ = $1; }
  | usingprintlist ';' printitem
  { $$ = AddToList($1, $3); }
  | usingprintlist ',' printitem
  { $$ = AddToList($1, $3); }
;

printlist:
/* empty */
    { $$ = AstCharItem('\n'); }
  | rawprintlist
    { $$ = AddToList($1, AstCharItem('\n')); }
  | rawprintlist ','
    { $$ = AddToList($1, AstCharItem('\t')); }
  | rawprintlist ';'
    { $$ = $1; }
  | BAS_USING BAS_STRING ';' usingprintlist
    { $$ = NewAST(AST_USING, $2, AddToList($4, AstCharItem('\n'))); }
  | BAS_USING BAS_STRING ';' usingprintlist ';'
    { $$ = NewAST(AST_USING, $2, $4); }
;

/* parse handle ',' position ',' data ',' size 
   position is optional
   size is optional, and the size of the data is used if it is omitted
*/
putgetargs:
  expritem ',' optzeroexpritem ',' expr
  {
      AST *filenum = $1;
      AST *position = $3;
      AST *data = $5;
      AST *amount = AstInteger(1);
      AST *size = NewAST(AST_SIZEOF, data, NULL);

      amount = NewAST(AST_EXPRLIST, amount, NULL);
      size = NewAST(AST_EXPRLIST, size, NULL);
      data = NewAST(AST_ADDROF, data, NULL);
      data = NewAST(AST_EXPRLIST, data, NULL);
      amount->right = size;
      data->right = amount;
      position->right = data;
      filenum->right = position;
      $$ = filenum;
  }
  | expritem ',' optzeroexpritem ',' expr ',' expritem optvar
  {
      AST *filenum = $1;
      AST *position = $3;
      AST *data = $5;
      AST *amount = $7;
      AST *var = $8;
      AST *size = NewAST(AST_SIZEOF, data, NULL);
      size = NewAST(AST_EXPRLIST, size, NULL);
      
      data = NewAST(AST_ADDROF, data, NULL);
      data = NewAST(AST_EXPRLIST, data, NULL);
      amount->right = size;
      data->right = amount;
      position->right = data;
      filenum->right = position;

      if (var) {
          filenum = AstAssign(var, filenum);
      }
      $$ = filenum;
  }
;

optzeroexpritem:
  /* nothing */
    {
        AST *zero = AstInteger(0);
        $$ = NewAST(AST_EXPRLIST, zero, NULL);
    }
  | expritem
    { $$ = $1; }
;

optvar:
  /* nothing */
    {
        $$ = NULL;
    }
  | ',' expr
    { $$ = $2; }
;


ifstmt:
  BAS_IF expr BAS_THEN eoln thenelseblock
    {
        $$ = NewCommentedAST(AST_IF, $2, $5, $1);
    }
  | BAS_IF expr BAS_THEN BAS_INTEGER
    {
        AST *stmtlist = NewCommentedStatement(
            NewAST(AST_GOTO, IntegerLabel($4), NULL));
        AST *elseblock = NewAST(AST_THENELSE, stmtlist, NULL);
        $$ = NewCommentedAST(AST_IF, $2, elseblock, $1);
    }
  | BAS_IF expr statement
    {
        AST *condition = $2;
        AST *stmtlist = NewCommentedStatement($3);
        AST *elseblock = NewAST(AST_THENELSE, stmtlist, NULL);
        $$ = NewCommentedAST(AST_IF, condition, elseblock, $1);
    }
  | BAS_IF expr BAS_THEN ifline
    {
        AST *elseblock = $4;
        $$ = NewCommentedAST(AST_IF, $2, elseblock, $1);
    }
;

thenelseblock:
  optstatementlist endif
    { $$ = NewAST(AST_THENELSE, $1, NULL); }
  | optstatementlist BAS_ELSE eoln optstatementlist endif
    { $$ = NewAST(AST_THENELSE, $1, $4); }
  | optstatementlist BAS_ELSE statement
    { $$ = NewAST(AST_THENELSE, $1, NewCommentedStatement($3)); }
;

endif:
  BAS_END
  | BAS_END BAS_IF
  | BAS_ENDIF
;

whilestmt:
    BAS_WHILE expr eoln { PushLoop(BAS_WHILE); } optstatementlist endwhile
    { AST *body = CheckYield($5);
      $$ = NewCommentedAST(AST_WHILE, $2, body, $1);
      PopLoop();
    }
;

endwhile:
  BAS_WEND
  | BAS_END BAS_WHILE
  | BAS_END
  ;

doloopstmt:
  BAS_DO BAS_WHILE expr eoln { PushLoop(BAS_DO); } optstatementlist BAS_LOOP
    { AST *body = CheckYield($6);
      AST *cond = $3;
      $$ = NewCommentedAST(AST_WHILE, cond, body, $1);
      PopLoop();
    }
  | BAS_DO BAS_UNTIL expr eoln {PushLoop(BAS_DO); } optstatementlist BAS_LOOP
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
  BAS_LOOP BAS_WHILE expr
    { $$ = $3; }
  | BAS_LOOP BAS_UNTIL expr
    { $$ = AstOperator(K_BOOL_NOT, NULL, $3); }
  | BAS_LOOP
    { $$ = AstInteger(1); }
;

selectstmt:
  BAS_SELECT BAS_CASE expr eoln casematchlist BAS_END BAS_SELECT
    { $$ = NewCommentedAST(AST_CASE, $3, $5, $1); }
  | BAS_ON expr BAS_GOTO exprlist
    { $$ = NewCommentedAST(AST_CASETABLE, $2, BuildOnGotoCases($4), $1); }
;

casematchlist:
  casematchitem
    { $$ = $1; }
  | casematchlist casematchitem
    { $$ = AddToList($1, $2); }
  ;

casematchitem:
  casematch eoln optstatementlist
    {
        AST *expr = $1;
        AST *stmts = $3;
        AST *firststmt;
        AST *breakstmt = NewAST(AST_ENDCASE, NULL, NULL);
        stmts = AddToList(stmts, NewAST(AST_STMTLIST, breakstmt, NULL));
        firststmt = stmts->left;
        if (expr->kind == AST_OTHER) {
            stmts->left = NewAST(AST_OTHER, firststmt, NULL);
        } else {
            stmts->left = NewAST(AST_CASEITEM, expr, firststmt);
        }
        $$ = stmts;
    }
;

casematch:
  BAS_CASE caselist
    {  $$ = $2; }
;

caseitem:
  expr
    {  $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  | expr BAS_TO expr
    {  $$ = NewAST(AST_EXPRLIST, NewAST(AST_RANGE, $1, $3), NULL); }
  | BAS_ELSE
    { $$ = NewAST(AST_OTHER, NULL, NULL); }
;

caselist:
  caseitem
    {  $$ = $1; }
  | caselist ',' caseitem
    {  $$ = AddToList($1, $3); }
;

//
// AST_FOR consists of:
// (AST_FOR (initstmt) (AST_TO (condtest (AST_STEP step body))))
//
forstmt:
    BAS_FOR { PushLoop(BAS_FOR); } BAS_IDENTIFIER '=' expr BAS_TO expr optstep eoln optstatementlist endfor
    {
      AST *from, *to, *step;
      AST *ident = $3;
      AST *closeident = $11;
      AST *declare;
      AST *loop;
      Symbol *sym = GetExplicitDeclares();
      int explicit_flag = EvalConstExpr((AST *)sym->val);

      if (0 == (explicit_flag & 0x8)) {
          /* create a WEAK definition for ident (it will not override any existing definition) */
          declare = NewAST(AST_DECLARE_VAR_WEAK, InferTypeFromName(ident), ident);
      } else {
          declare = 0;
      }
      step = NewAST(AST_STEP, $8, $10);
      to = NewAST(AST_TO, $7, step);
      from = NewAST(AST_FROM, $5, to);
      loop = NewCommentedAST(AST_COUNTREPEAT, $3, from, $1);
      // validate the "next i"
      if (closeident && !AstMatch(ident, closeident)) {
          SETCOLOR(PRINT_ERROR);
          ERRORHEADER(current->Lptr->fileName, current->Lptr->lineCounter, "error");
          fprintf(stderr, "Wrong variable in next: expected %s, saw %s\n", ident->d.string, closeident->d.string);
          gl_errors++;
          RESETCOLOR();
      }
      loop = NewAST(AST_STMTLIST, loop, NULL);
      if (declare) {
          loop = NewAST(AST_STMTLIST, declare,
                        NewAST(AST_STMTLIST, loop, NULL));
      }
      $$ = loop;
      PopLoop();
    }
;

endfor:
  BAS_NEXT
    { $$ = NULL; }
  | BAS_NEXT BAS_IDENTIFIER
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
  BAS_END BAS_TRY
;

exitstmt:
  BAS_EXIT BAS_FUNCTION
    { $$ = NewAST(AST_RETURN, NULL, NULL); }
  | BAS_EXIT BAS_SUB
    { $$ = NewAST(AST_RETURN, NULL, NULL); }
  | BAS_EXIT BAS_FOR
    {
        int curLoop = GetCurrentLoop(BAS_FOR);
        AST *modifier = NULL;
        if (!curLoop) {
            SYNTAX_ERROR("'exit for' is not inside a for loop");
        } else {
            modifier = AstInteger(curLoop);
        }
        $$ = NewAST(AST_QUITLOOP, modifier, NULL);
    }
  | BAS_EXIT BAS_WHILE
    {
        int curLoop = GetCurrentLoop(BAS_WHILE);
        AST *modifier = NULL;
        if (!curLoop) {
            SYNTAX_ERROR("'exit while' is not inside a while loop");
        } else {
            modifier = AstInteger(curLoop);
        }
        $$ = NewAST(AST_QUITLOOP, modifier, NULL);
    }
  | BAS_EXIT BAS_DO
    {
        int curLoop = GetCurrentLoop(BAS_DO);
        AST *modifier = NULL;
        if (!curLoop) {
            SYNTAX_ERROR("'exit do' is not inside a do loop");
        } else {
            modifier = AstInteger(curLoop);
        }
        $$ = NewAST(AST_QUITLOOP, modifier, NULL);
    }
  | BAS_EXIT BAS_LOOP
    {
        int curLoop = GetCurrentLoop(BAS_DO);
        AST *modifier = NULL;
        if (!curLoop) {
            SYNTAX_ERROR("'exit loop' is not inside a do loop");
        } else {
            modifier = AstInteger(curLoop);
        }
        $$ = NewAST(AST_QUITLOOP, modifier, NULL);
    }
  | BAS_EXIT
    {
        int curLoop = GetCurrentLoop(0);
        AST *modifier = NULL;
        if (!curLoop) {
            SYNTAX_ERROR("'exit' is not inside a loop");
        } else {
            modifier = AstInteger(curLoop);
        }
        $$ = NewAST(AST_QUITLOOP, modifier, NULL);
    }
  | BAS_CONTINUE BAS_FOR
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
  | BAS_CONTINUE BAS_WHILE
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
  | BAS_CONTINUE BAS_DO
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
  | BAS_CONTINUE
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

topdecl:
  BAS_DEFSNG deflist
    {
      $$ = DefineDefaultVarTypes($2, ast_type_float);
    }  
  | BAS_DEFINT deflist
    {
      $$ = DefineDefaultVarTypes($2, ast_type_long);
    }
  | dimension
    {
        AST *ast = $1;
        if (ast->kind == AST_GLOBALVARS) {
            DeclareBASICSharedVariables(ast->left);
        } else if (ast->kind == AST_REGISTERVARS) {
            DeclareBASICRegisterVariables(ast->left);
        } else {
            DeclareBASICMemberVariables(ast);
        }
    }
  | BAS_OPTION BAS_IDENTIFIER optexprlist
    {
      $$ = HandleBASICOption($2, $3);
    }
  | classdecl
  | typedecl
  | funcdecl
  | subdecl
  | constdecl
  | functemplate
;

defitem:
  BAS_IDENTIFIER '-' BAS_IDENTIFIER
     { $$ = NewAST(AST_SEQUENCE, $1, $3); }
;

deflist:
  defitem
    { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  | deflist ',' defitem
    {
        AST *list = $1;
        AST *item = NewAST(AST_EXPRLIST, $3, NULL);
        $$ = AddToListEx(list, item, (AST **)&list->d.ptr);
    }
;

varexpr:
  BAS_IDENTIFIER
    { $$ = $1; }
  | BAS_IDENTIFIER '[' expr ']'
    { $$ = NewAST(AST_ARRAYREF, $1, $3); }
  | varexpr '(' ')'
    { $$ = NewAST(AST_FUNCCALL, $1, NULL); }
  | varexpr '(' exprlist ')'
    { $$ = NewAST(AST_FUNCCALL, $1, $3); }
  | varexpr '.' BAS_IDENTIFIER
    { $$ = NewAST(AST_METHODREF, $1, $3); }
  | varexpr '.' BAS_PROGRAM
    { $$ = NewAST(AST_METHODREF, $1, AstIdentifier("program")); }
  | basetypename '.' BAS_IDENTIFIER
    { $$ = NewAST(AST_METHODREF, $1, $3); }
  | basetypename '.' BAS_PROGRAM
    { $$ = NewAST(AST_METHODREF, $1, AstIdentifier("program")); }
;

varassigntarget:
  varexpr
    { $$ = $1; }
  | BAS_EMPTY
    { $$ = NewAST(AST_EMPTY, NULL, NULL); }
;

pinrange:
  expr
    { $$ = NewAST(AST_RANGE, $1, NULL); }
  | expr ',' expr
    { $$ = NewAST(AST_RANGE, $1, $3); }
;

register_expr:
  BAS_INPUT '(' pinrange ')'
    { $$ = GetPinRange("ina", "inb", $3); }
  | BAS_OUTPUT '(' pinrange ')'
    { $$ = GetPinRange("outa", "outb", $3); }
  | BAS_DIRECTION '(' pinrange ')'
    { $$ = GetPinRange("dira", "dirb", $3); }
;

/* expressions that do not start with '(' */
np_primary_expr:
  varexpr
    { $$ = $1; }
  | register_expr
    { $$ = $1; }
  | BAS_INTEGER
    { $$ = $1; }
  | BAS_FLOAT
    { $$ = $1; }
  | BAS_NIL
    { $$ = AstBitValue(0); }
  | BAS_SELF
    { $$ = NewAST(AST_SELF, NULL, NULL); }
  | BAS_FUNC_NAME
    { $$ = NewAST(AST_FUNC_NAME, NULL, NULL); }
  | BAS_STRING
    { $$ = NewAST(AST_STRINGPTR,
                  NewAST(AST_EXPRLIST, $1, NULL), NULL); }
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
  | BAS_ALLOCA '(' expr ')'
    {
        $$ = NewAST(AST_ALLOCA, ast_type_ptr_void, $3);
    }
  | BAS_SIZEOF expr
    {
        $$ = NewAST(AST_SIZEOF, $2, NULL);
    }
;

primary_expr:
  np_primary_expr
    { $$ = $1; }
  | '(' expr ')'
    { $$ = $2; }
;

unary_op:
  '-'
    { $$ = AstOperator(K_NEGATE, NULL, NULL); }
  | BAS_NOT
    { $$ = AstOperator(K_BIT_NOT, NULL, NULL); }
  | BAS_ABS
    { $$ = AstOperator(K_ABS, NULL, NULL); }
  | BAS_ASC
    { $$ = AstOperator(K_ASC, NULL, NULL); }
  | BAS_SQRT
    { $$ = AstOperator(K_SQRT, NULL, NULL); }
;

pseudofunc_expr:
  BAS_CAST_INT '(' expr ')'
    {
        AST *orig = $3;
        $$ = NewAST(AST_CAST, ast_type_long, orig);
    }
  | BAS_CAST '(' typename ',' expr ')'
    {
        $$ = NewAST(AST_CAST, $3, $5);
    }
  | BAS_CPU '(' exprlist ')'
    {
        AST *elist;
        AST *immval = AstInteger(0x1e); // works to cognew both P1 and P2
        elist = NewAST(AST_EXPRLIST, immval, NULL);
        elist = AddToList(elist, $3);
        $$ = NewAST(AST_COGINIT, elist, NULL);
    }
  | BAS_HASMETHOD '(' typename ',' BAS_IDENTIFIER ')'
    {
        AST *typnam = $3;
        AST *ident = $5;
        $$ = NewAST(AST_HASMETHOD, typnam, ident);
    }
  | BAS_SAMETYPES '(' typename ',' typename ')'
    {
        AST *expr = $3;
        AST *typnam = $5;
        $$ = NewAST(AST_SAMETYPES, expr, typnam);
    }
  | BAS_SAMETYPES '(' expr ',' typename ')'
    {
        SYNTAX_ERROR("SameTypes requires two type names");
        $$ = AstInteger(0);
    }
  | '@' unary_expr
    { $$ = NewAST(AST_ADDROF, $2, NULL); }
;

unary_expr:
  primary_expr
    { $$ = $1; }
  | pseudofunc_expr
    { $$ = $1; }
  | '+' unary_expr
    { $$ = $2; }
  | unary_op unary_expr
    { $$ = $1; $$->right = $2; }
;

np_unary_expr:
  np_primary_expr
    { $$ = $1; }
  | pseudofunc_expr
    { $$ = $1; }  
  | '+' unary_expr
    { $$ = $2; }
  | unary_op unary_expr
    { $$ = $1; $$->right = $2; }
;

lambdaexpr:
  BAS_FUNCTION '(' paramdecl ')' expr
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
  | '[' paramdecl ':' statementlist ']'
    {
      AST *params = $2;
      AST *rettype = ast_type_void;
      AST *body = $4;
      AST *functype = NewAST(AST_FUNCTYPE, rettype, params);
      $$ = NewAST(AST_LAMBDA, functype, body);
    }
  | '[' paramdecl ':' optstatementlist BAS_GE expr opttype ']'
    {
      AST *params = $2;
      AST *rettype = $7;
      AST *body = $4;
      AST *retstmt = NewCommentedStatement(AstReturn($6, NULL));
      AST *functype = NewAST(AST_FUNCTYPE, rettype, params);
      body = AddToList(body, retstmt);
      $$ = NewAST(AST_LAMBDA, functype, body);
    }
;
opttype:
  /* nothing */
    { $$ = NULL; }
  | BAS_AS typename
    { $$ = $2; }
;

mult_op:
  '*'
    { $$ = AstOperator('*', NULL, NULL); }
  | '/'
    { $$ = AstOperator('/', NULL, NULL); }
  | BAS_MOD
    { $$ = AstOperator(K_MODULUS, NULL, NULL); }
  | BAS_SHL
    { $$ = AstOperator(K_SHL, NULL, NULL); }
  | BAS_SHR
    { $$ = AstOperator(K_SAR, NULL, NULL); }
;

power_expr:
  unary_expr
    { $$ = $1; }
  | power_expr '^' unary_expr
    {
        $$ = AstOperator(K_POWER, $1, $3);
    }
;

np_power_expr:
  np_unary_expr
    { $$ = $1; }
  | np_power_expr '^' np_unary_expr
    {
        $$ = AstOperator(K_POWER, $1, $3);
    }
;
  
mult_expr:
  power_expr
    { $$ = $1; }
  | mult_expr mult_op power_expr
    {
        $$ = $2;
        $$->left = $1;
        $$->right = $3;
    }
;
np_mult_expr:
  np_power_expr
    { $$ = $1; }
  | np_mult_expr mult_op np_power_expr
    {
        $$ = $2;
        $$->left = $1;
        $$->right = $3;
    }
;

add_expr:
  mult_expr
    { $$ = $1; }
  | add_expr '+' mult_expr  
    { $$ = AstOperator('+', $1, $3); }
  | add_expr '-' mult_expr  
    { $$ = AstOperator('-', $1, $3); }
;

np_add_expr:
  np_mult_expr
    { $$ = $1; }
  | np_add_expr '+' mult_expr  
    { $$ = AstOperator('+', $1, $3); }
  | np_add_expr '-' mult_expr  
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
    { $$ = AstOperator(K_NE, $1, $3); }
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

bool_expr:
  bit_expr
    { $$ = $1; }
  | bool_expr BAS_ANDALSO bit_expr
    { $$ = AstOperator(K_BOOL_AND, $1, $3); }
  | bool_expr BAS_ORELSE bit_expr
    { $$ = AstOperator(K_BOOL_OR, $1, $3); }
;

expr:
  bool_expr
    { $$ = $1; }
  | lambdaexpr
    { $$ = $1; }
;

np_expr: 
  np_add_expr
  | lambdaexpr
  ;

optexprlist:
    /* empty */
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

np_expritem:
  np_expr
   { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
;
np_exprlist:
  np_expritem
    { $$ = $1; }
  | np_expritem ',' exprlist
    { $$ = AddToList($1, $3); }
;

subbody:
  optstatementlist endsub
  { $$ = $1; }
  ;

endsub:
  BAS_END
  | BAS_END BAS_SUB
;

funcbody:
  optstatementlist endfunc
  { $$ = $1; }
  ;

endfunc:
  BAS_END
  | BAS_END BAS_FUNCTION
;

attributes:
  /* empty */
    { $$ = 0; }
  | BAS_FOR BAS_STRING
    {
        AST *ast = $2;
        ast->kind = AST_ANNOTATION;
        $$ = ast;
    }
;
    
subdecl:
  BAS_SUB attributes BAS_IDENTIFIER '(' paramdecl ')' eoln subbody
  {
    AST *attrib = $2;
    AST *ident = $3;
    AST *parms = $5;
    AST *body = $8;
    AST *funcdecl = NewAST(AST_FUNCDECL, ident, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, parms, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    DeclareFunction(current, ast_type_void, 1, funcdef, body, attrib, $1);
  }
  | BAS_SUB attributes BAS_IDENTIFIER paramdecl eoln subbody
  {
    AST *attrib = $2;
    AST *ident = $3;
    AST *parms = $4;
    AST *body = $6;
    AST *funcdecl = NewAST(AST_FUNCDECL, ident, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, parms, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    DeclareFunction(current, ast_type_void, 1, funcdef, body, attrib, $1);
  }
  | BAS_DECLARE BAS_SUB BAS_IDENTIFIER BAS_LIB BAS_STRING '(' paramdecl ')'
     {
         AST *attrib = NULL;
         AST *name = $3;
         AST *parms = $7;
         AST *rettype = ast_type_void;
         AST *body = $5;
         AST *funcdecl = NewAST(AST_FUNCDECL, name, NULL);
         AST *funcvars = NewAST(AST_FUNCVARS, parms, NULL);
         AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
         
         DeclareFunction(current, rettype, 1, funcdef, body, attrib, $1);
     }
  ;

funcdecl:
  BAS_FUNCTION attributes BAS_IDENTIFIER '(' paramdecl ')' eoln funcbody
  {
    AST *attrib = $2;
    AST *name = $3;
    AST *parms = $5;
    AST *body = $8;
    AST *funcdecl = NewAST(AST_FUNCDECL, name, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, parms, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    AST *rettype = InferTypeFromName(name);
    DeclareFunction(current, rettype, 1, funcdef, body, attrib, $1);
  }
  | BAS_FUNCTION attributes BAS_IDENTIFIER '(' paramdecl ')' BAS_AS typelist eoln funcbody
  {
    AST *attrib = $2;
    AST *name = $3;
    AST *parms = $5;
    AST *rettype = $8;
    AST *body = $10;
    AST *funcdecl = NewAST(AST_FUNCDECL, name, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, parms, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    DeclareFunction(current, rettype, 1, funcdef, body, attrib, $1);
  }
  | BAS_DECLARE BAS_FUNCTION BAS_IDENTIFIER BAS_LIB BAS_STRING '(' paramdecl ')' BAS_AS typelist
     {
         AST *attrib = NULL;
         AST *name = $3;
         AST *parms = $7;
         AST *rettype = $10;
         AST *body = $5;
         AST *funcdecl = NewAST(AST_FUNCDECL, name, NULL);
         AST *funcvars = NewAST(AST_FUNCVARS, parms, NULL);
         AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
         
         DeclareFunction(current, rettype, 1, funcdef, body, attrib, $1);
     }
  | BAS_DEF BAS_IDENTIFIER '(' paramdecl ')' '=' expr
  {
    AST *name = $2;
    AST *funcdecl = NewAST(AST_FUNCDECL, name, NULL);
    AST *funcvars = NewAST(AST_FUNCVARS, $4, NULL);
    AST *funcdef = NewAST(AST_FUNCDEF, funcdecl, funcvars);
    AST *rettype = InferTypeFromName(name);
    AST *retval = $7;
    AST *body;

    body = NewAST(AST_STMTLIST,
                  NewAST(AST_RETURN, retval, NULL),
                  NULL);
    DeclareFunction(current, rettype, 1, funcdef, body, NULL, $1);
  }
  ;

functemplate:
    templateheader BAS_FUNCTION BAS_IDENTIFIER '(' paramdecl ')' BAS_AS typelist eoln funcbody
    {
        AST *name = $3;
	AST *types = $1;
        AST *paramvars = $5;
        AST *rettype = $8;
	AST *functype = NewAST(AST_FUNCTYPE, rettype, paramvars);
	AST *body = $10;
	AST *top_decl;
	top_decl = NewAST(AST_LISTHOLDER, functype,
			  NewAST(AST_LISTHOLDER, name,
				 NewAST(AST_LISTHOLDER, body, NULL)));
        PopCurrentTypes();
	top_decl = NewAST(AST_FUNC_TEMPLATE, types, top_decl);
	DeclareFunctionTemplate(current, top_decl);
    }
    | templateheader BAS_SUB BAS_IDENTIFIER '(' paramdecl ')' eoln subbody
    {
        AST *name = $3;
	AST *types = $1;
        AST *paramvars = $5;
        AST *rettype = ast_type_void;
	AST *functype = NewAST(AST_FUNCTYPE, rettype, paramvars);
	AST *body = $8;
	AST *top_decl;
	top_decl = NewAST(AST_LISTHOLDER, functype,
			  NewAST(AST_LISTHOLDER, name,
				 NewAST(AST_LISTHOLDER, body, NULL)));
        PopCurrentTypes();
	top_decl = NewAST(AST_FUNC_TEMPLATE, types, top_decl);
	DeclareFunctionTemplate(current, top_decl);
    }
    ;

templateheader:
    BAS_ANY '(' identlist ')'
    {
      PushCurrentTypes();
      $$ = AddTemplateTypes($3);
    }
    ;

identlist:
  BAS_IDENTIFIER
    {
      $$ = NewAST(AST_LISTHOLDER, $1, NULL);
    }
  | identlist ',' BAS_IDENTIFIER
    {
      AST *rhs = NewAST(AST_LISTHOLDER, $3, NULL);
      $$ = AddToList($1, rhs);
    }
;

assignitem:
  BAS_IDENTIFIER '=' expr
    {
        AST *assign = NewAST(AST_ASSIGN, $1, $3);
        $$ = NewAST(AST_LISTHOLDER, assign, NULL);
    }
;

assignlist:
  assignitem
    { $$ = $1; }
  | assignitem ',' assignlist
    {
        AST *first = $1;
        AST *rest = $3;
        first->right = rest;
        $$ = first;
    }
;

optobjparams:
  /* nothing */
    { $$ = NULL; }
  | BAS_WITH assignlist
    {
        $$ = $2;
    }
;

classdecl:
  BAS_CLASS BAS_IDENTIFIER BAS_USING BAS_STRING optobjparams
    {
        AST *params = $5;
        AST *newobj = NewAbstractObjectWithParams( $2, $4, 1, params );
        current->objblock = AddToList(current->objblock, newobj);
        AddSymbol(currentTypes, $2->d.string, SYM_TYPEDEF, newobj, NULL);
        $$ = NULL;
    }
  | classheader classdecllist
  ;

classheader:
  BAS_CLASS BAS_IDENTIFIER eoln
    {
      AST *ident = $2;
      const char *classname = ident->d.string;
      Module *P = NewModule(classname, current->curLanguage);
      AST *newobj = NewAbstractObject( $2, NULL, 0 );
      newobj->d.ptr = P;
      AddSymbol(currentTypes, $2->d.string, SYM_TYPEDEF, newobj, NULL);
      P->Lptr = current->Lptr;
      P->subclasses = current->subclasses;
      current->subclasses = P;
      P->superclass = current;
      P->fullname = current->fullname; // for finding "class using"
      current = P;
      $$ = NULL;
    }
  | BAS_UNION BAS_IDENTIFIER eoln
    {
      AST *ident = $2;
      const char *classname = ident->d.string;
      Module *P = NewModule(classname, current->curLanguage);
      AST *newobj = NewAbstractObject( $2, NULL, 0 );
      newobj->d.ptr = P;
      AddSymbol(currentTypes, $2->d.string, SYM_TYPEDEF, newobj, NULL);
      P->Lptr = current->Lptr;
      P->subclasses = current->subclasses;
      current->subclasses = P;
      P->superclass = current;
      P->fullname = current->fullname; // for finding "class using"
      P->isUnion = 1;
      current = P;
      $$ = NULL;
    }  
  ;

classdecllist:
  classdeclitem classend
  | classdeclitem eoln classdecllist
  ;

classend:
  BAS_END BAS_CLASS
    {
      if (!current->superclass || current->isUnion) {
        SYNTAX_ERROR("END CLASS not inside class");
      } else {
        current = current->superclass;
      }
    }
  | BAS_END BAS_UNION
    {
      if (!current->superclass || !current->isUnion) {
        SYNTAX_ERROR("END UNION not inside union");
      } else {
        current = current->superclass;
      }
    }  
;

classdeclitem:
| dimlist BAS_AS typename
    {
        AST *ast = NewAST(AST_DECLARE_VAR, $3, $1);
        DeclareBASICMemberVariables(ast);
    }
| dimension
    {
        AST *ast = $1;
        if (ast->kind == AST_GLOBALVARS) {
            DeclareBASICSharedVariables(ast->left);
        } else {
            DeclareBASICMemberVariables(ast);
        }
    }
| funcdecl
| subdecl
| functemplate
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
  BAS_TYPE BAS_IDENTIFIER BAS_AS typename
    {
        AddSymbol(currentTypes, $2->d.string, SYM_TYPEDEF, $4, NULL);
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
        $$ = MakeArrayType($1, size);
        $$->d.ptr = (AST *)sym->val;
    }
  ;

typelist:
  typename
    { $$ = $1; }
  | typelist ',' typename
    {
        AST *list = $1;
        AST *added = $3;
        if (list->kind != AST_TUPLE_TYPE) {
            list = NewAST(AST_TUPLE_TYPE, list, NULL);
        }
        $$ = AddToList(list, NewAST(AST_TUPLE_TYPE, added, NULL));
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
  | BAS_LONGINT
    { $$ = ast_type_long64; }
  | BAS_BYTE
    { $$ = ast_type_signed_byte; }
  | BAS_SHORT
    { $$ = ast_type_signed_word; }
  | BAS_ULONG
    { $$ = ast_type_unsigned_long; }
  | BAS_ULONGINT
    { $$ = ast_type_unsigned_long64; }
  | BAS_INTEGER_KW
    { $$ = ast_type_long; }
  | BAS_BOOLEAN
    { $$ = ast_type_long; }
  | BAS_UINTEGER
    { $$ = ast_type_unsigned_long; }
  | BAS_SINGLE
    { $$ = ast_type_float; }
  | BAS_DOUBLE
    { $$ = ast_type_float; }
  | BAS_FIXED '(' BAS_INTEGER ')'
    {
        AST *sizeexpr = $3;
        int siz = sizeexpr->d.ival;
        if (siz < 1 || siz > 31) {
            SYNTAX_ERROR("Fixed point size %d is out of range 1..31", siz);
            siz = 16;
        }
        SYNTAX_ERROR("Fixed point types not implemented yet");
        $$ = ast_type_float;
    }
  | BAS_STRING_KW
    { $$ = ast_type_string; }
  | BAS_ANY
    { $$ = ast_type_generic; }
  | BAS_TYPENAME
    { $$ = $1; }
  | BAS_CONST basetypename
    { $$ = NewAST(AST_MODIFIER_CONST, $2, NULL); }
  | BAS_FUNCTION '(' paramdecl ')' BAS_AS basetypename
    { $$ = NewAST(AST_PTRTYPE, NewAST(AST_FUNCTYPE, $6, $3), NULL); }
  | BAS_SUB '(' paramdecl ')'
    { $$ = NewAST(AST_PTRTYPE, NewAST(AST_FUNCTYPE, ast_type_void, $3), NULL); }
  | BAS_CLASS BAS_USING BAS_STRING optobjparams
    {
        AST *params = $4;
        AST *tempnam = NewAST(AST_IDENTIFIER, NULL, NULL);
        const char *name = NewTemporaryVariable("_class_", NULL);
        AST *newobj;

        tempnam->d.string = name;
        newobj = NewAbstractObjectWithParams( tempnam, $3, 1, params );        
        current->objblock = AddToList(current->objblock, newobj);
        $$ = newobj;
    }
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
  paramvar
    { $$ = AdjustParamType($1); }
  | BAS_BYREF paramvar
    { $$ = AdjustParamForByRef($2); }
  | BAS_BYVAL paramvar
    { $$ = AdjustParamForByVal($2); }
;

paramvar:
  BAS_IDENTIFIER
    { $$ = NewAST(AST_DECLARE_VAR, InferTypeFromName($1), $1); }
  | BAS_IDENTIFIER '=' expr
    { $$ = NewAST(AST_DECLARE_VAR, InferTypeFromName($1), AstAssign($1, $3)); }
  | BAS_IDENTIFIER BAS_AS typename
    { $$ = NewAST(AST_DECLARE_VAR, $3, $1); }
  | BAS_IDENTIFIER '(' ')' BAS_AS typename
    {
        AST *typ = $5;
        typ = MakeArrayType(typ, AstInteger(0));
        $$ = NewAST(AST_DECLARE_VAR, typ, $1);
    }
  | BAS_IDENTIFIER '(' expr ')' BAS_AS typename
    {
        AST *siz = $3;
        AST *typ = $6;
        typ = MakeArrayType(typ, siz);
        $$ = NewAST(AST_DECLARE_VAR, typ, $1);
    }
  | BAS_IDENTIFIER '=' expr BAS_AS typename
    { $$ = NewAST(AST_DECLARE_VAR, $5, AstAssign($1, $3)); }
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
  | BAS_DIM BAS_REGISTER dimlist
    { $$ = NewAST(AST_REGISTERVARS, NewAST(AST_DECLARE_VAR, NULL, $3), NULL); }
  | BAS_DIM BAS_REGISTER dimlist BAS_AS typename
    { $$ = NewAST(AST_REGISTERVARS, NewAST(AST_DECLARE_VAR, $5, $3), NULL); }
  | BAS_DIM BAS_REGISTER BAS_AS typename dimlist
    { $$ = NewAST(AST_REGISTERVARS, NewAST(AST_DECLARE_VAR, $4, $5), NULL); }
  | BAS_DECLARE BAS_IDENTIFIER BAS_ALIAS BAS_IDENTIFIER opt_type_as
    {
        AST *newname = $2;
        AST *oldname = $4;
        AST *newtype = $5;
        if (newtype) {
            oldname = NewAST(AST_TYPEDEF, newtype, oldname);
        }
        $$ = NewAST(AST_DECLARE_ALIAS, newname, oldname);
    }
  | BAS_DECLARE BAS_IDENTIFIER BAS_ALIAS BAS_INTEGER BAS_AS typename
    {
        AST *newname = $2;
        AST *addr = $4;
        AST *newtype = $6;

        addr = NewAST(AST_MEMREF, newtype, addr);
        addr = BASICArrayRef(addr, AstInteger(0));
        addr = NewAST(AST_TYPEDEF, newtype, addr);
        $$ = NewAST(AST_DECLARE_ALIAS, newname, addr);
    }
  ;

opt_type_as :
  /* nothing */
    { $$ = NULL; }
  | BAS_AS typename
    { $$ = $2; }
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
  | BAS_IDENTIFIER '(' expr ',' expr ')'
    {
        Symbol *sym = GetCurArrayBase();
        AST *ident = $1;
        AST *base = (AST *)sym->val;
        AST *size1 = $3;
        AST *size2 = $5;
        AST *sizelist;
        AST *decl;
        size1 = AstOperator('-', size1, AstOperator('-', base, AstInteger(1)));
        size2 = AstOperator('-', size2, AstOperator('-', base, AstInteger(1)));
        sizelist = NewAST(AST_EXPRLIST, size1,
                          NewAST(AST_EXPRLIST, size2, NULL));
        decl = NewAST(AST_ARRAYDECL, ident, sizelist);
        decl->d.ptr = base;
        $$ = decl;
    }
  | BAS_IDENTIFIER '(' expr BAS_TO expr ',' expr BAS_TO expr ')'
    {
        AST *ident = $1;
        AST *base1 = $3;
        AST *size1 = $5;
        AST *base2 = $7;
        AST *size2 = $9;
        AST *sizelist;
        AST *decl;
        size1 = AstOperator('-', size1, AstOperator('-', base1, AstInteger(1)));
        size2 = AstOperator('-', size2, AstOperator('-', base2, AstInteger(1)));
        sizelist = NewAST(AST_EXPRLIST, size1,
                          NewAST(AST_EXPRLIST, size2, NULL));
        decl = NewAST(AST_ARRAYDECL, ident, sizelist);
        if (!AstMatch(base1, base2)) {
            SYNTAX_ERROR("Base values for multi-dimensional array indices must match");
        }
        decl->d.ptr = base1;
        $$ = decl;
    }
;

asmstmt:
  BAS_ASM eoln asmlist BAS_END BAS_ASM
      {
          // integer 0 means default asm
          $$ = NewCommentedAST(AST_INLINEASM, $3, AstInteger(0), $1);
      }
  | BAS_CONST BAS_ASM eoln asmlist BAS_END BAS_ASM
      {
          // integer 1 means const asm
          $$ = NewCommentedAST(AST_INLINEASM, $4, AstInteger(1), $1);
      }
  | BAS_CPU BAS_ASM eoln asmlist BAS_END BAS_ASM
      {
          // integer 3 means const asm execute from FCACHE
          $$ = NewCommentedAST(AST_INLINEASM, $4, AstInteger(3), $1);
      }
  | BAS_SHARED BAS_ASM eoln asmlist BAS_END BAS_ASM
      { current->datblock = AddToListEx(current->datblock, $4, &current->datblock_tail); $$ = 0;}
  ;

  | BAS_ASM BAS_SHARED eoln asmlist BAS_END BAS_ASM
      { current->datblock = AddToListEx(current->datblock, $4, &current->datblock_tail); $$ = 0;}
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
  | BAS_BYTE operandlist BAS_EOLN
    { $$ = NewCommentedAST(AST_BYTELIST, $2, NULL, $1); }
  | BAS_WORD BAS_EOLN
    { $$ = NewCommentedAST(AST_WORDLIST, NULL, NULL, $1); }
  | BAS_WORD operandlist BAS_EOLN
    { $$ = NewCommentedAST(AST_WORDLIST, $2, NULL, $1); }
  | BAS_LONG BAS_EOLN
    { $$ = NewCommentedAST(AST_LONGLIST, NULL, NULL, $1); }
  | BAS_LONG operandlist BAS_EOLN
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
  | BAS_ORG BAS_EOLN
    { $$ = NewCommentedAST(AST_ORG, NULL, NULL, $1); }
  | BAS_ORG pasmexpr BAS_EOLN
    { $$ = NewCommentedAST(AST_ORG, $2, NULL, $1); }
  | BAS_ORGH BAS_EOLN
    { $$ = NewCommentedAST(AST_ORGH, NULL, NULL, $1); }
  | BAS_ORGH pasmexpr BAS_EOLN
    { $$ = NewCommentedAST(AST_ORGH, $2, NULL, $1); }
  | BAS_ORGF pasmexpr BAS_EOLN
    { $$ = NewCommentedAST(AST_ORGF, $2, NULL, $1); }
  | BAS_RES pasmexpr BAS_EOLN
    { $$ = NewCommentedAST(AST_RES, $2, NULL, $1); }
  | BAS_FIT pasmexpr BAS_EOLN
    { $$ = NewCommentedAST(AST_FIT, $2, NULL, $1); }
  | BAS_FIT BAS_EOLN
    { $$ = NewCommentedAST(AST_FIT, AstInteger(0x1f0), NULL, $1); }
  | BAS_FILE BAS_STRING BAS_EOLN
    { $$ = NewCommentedAST(AST_FILE, GetFullFileName($2), NULL, $1); }
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
 | pasmexpr '+' '+'
   { $$ = NewAST(AST_EXPRLIST, AstOperator(K_INCREMENT, $1, NULL), NULL); } 
;

pasmexpr:
  expr
    { $$ = $1; }
  | '\\' expr
    { $$ = AstCatch($2); }
  | BAS_HWREG
    { $$ = $1; }
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
    
    SETCOLOR(PRINT_ERROR);
    ERRORHEADER(current->Lptr->fileName, current->Lptr->lineCounter, "error");

    if (!strcmp(msg, "syntax error, unexpected $end")) {
        fprintf(stderr, "unexpected end of input");
        msg = "";
    }
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
    RESETCOLOR();
}
