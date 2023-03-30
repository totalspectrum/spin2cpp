/*
 * Spin compiler parser
 * Copyright (c) 2011-2023 Total Spectrum Software Inc.
 * See the file COPYING for terms of use.
 */

%pure-parser

%{
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "spinc.h"

#define SPINYYSTYPE AST*
#undef  YYSTYPE
#define YYSTYPE SPINYYSTYPE
    
/* Yacc functions */
    void spinyyerror(const char *);
    int spinyylex();

    extern int gl_errors;

    extern AST *last_ast;
    
AST *
SpinRetType(AST *funcdef)
{
    AST *resultvars = NULL;
    
    if (funcdef->kind != AST_FUNCDEF) return NULL;
    funcdef = funcdef->left;
    if (funcdef->kind != AST_FUNCDECL) return NULL;
    resultvars = funcdef->right;
    if (resultvars && resultvars->kind == AST_DECLARE_VAR) {
        return resultvars->left;
    }
    funcdef = funcdef ->left;
    if (funcdef->kind != AST_IDENTIFIER) return NULL;
    if (strrchr(funcdef->d.string, '$') != NULL)
        return ast_type_string;
    return NULL;
}

// in common.c
extern AST *GenericFunctionPtr(int numresults);

static AST *
MakeFunccall(AST *func, AST *params, AST *numresults)
{
    if (numresults && numresults->kind == AST_INTEGER) {
        int paramcount;
        paramcount = numresults->d.ival;
        func = NewAST(AST_CAST, GenericFunctionPtr(paramcount), func);
    }
    return NewAST(AST_FUNCCALL, func, params);
}

// special handling for things like "abc" | x
// in Spin this is the same as "ab", "c" | x

static int IsLongString(AST *ast)
{
    if (ast && ast->kind == AST_STRING) {
        return strlen(ast->d.string) > 1;
    }
    return 0;
}

// convert a pin range like 57..46 to an ADDPINS expression
static AST *
ConvertPinRange(AST *exprlist)
{
    AST *expr;
    AST *lhs, *rhs;
    if (!exprlist || !exprlist->left) {
        return exprlist;
    }
    expr = exprlist->left;
    if (expr->kind != AST_RANGE) {
        return exprlist;
    }
    lhs = expr->left;
    rhs = expr->right;
    if (!rhs) {
        exprlist->left = rhs;
        return exprlist;
    }
    // convert A..B to B ADDPINS  (B-A)
    lhs = AstOperator('-', lhs, rhs);
    expr = AstOperator('|', rhs, AstOperator(K_SHL, lhs, AstInteger(6)));
    exprlist->left = expr;
    return exprlist;
}

// Fix up an expression list
// convert any long string operator entries by folding the operation
// into the first/last character of the string

static AST *
FixupList(AST *list)
{
    AST *item, *left, *right;
    AST *origlist = list;
    char *shortstr;
    char *singlechar;
    int slen;
    int op;
    
    while (list && list->kind == AST_EXPRLIST) {
        item = list->left;
        if (item && item->kind == AST_OPERATOR) {
            op = item->d.ival;
            left = item->left;
            right = item->right;
            if (IsLongString(left)) {
                if (IsLongString(right)) {
                    SYNTAX_ERROR("unable to apply operator to two strings");
                } else {
                    // convert to AST_EXPRLIST( most of left, left_last_char OP right )
                    shortstr = strdup(left->d.string);
                    slen = strlen(left->d.string);
                    singlechar = strdup(shortstr + slen - 1);
                    shortstr[slen - 1] = 0;
                    left = AstPlainString(shortstr);
                    right = AstOperator(op, AstPlainString(singlechar), right);
                    list->left = left;
                    list->right = NewAST(AST_EXPRLIST, right, list->right);
                }
            } else if (IsLongString(right)) {
                shortstr = strdup(right->d.string+1);
                singlechar = (char *)calloc(1, 2);
                singlechar[0] = right->d.string[0];
                left = AstOperator(op, left, AstPlainString(singlechar));
                right = AstPlainString(shortstr);
                list->left = left;
                list->right = NewAST(AST_EXPRLIST, right, list->right);
            }
        }
        list = list->right;
    }
    return origlist;
}

#define DBG_BITS_DELAY 0x1000

static struct s_dbgfmt {
    const char *name;
    const char *cfmt;
    int bits;
} dbgfmt[] = {
    { "uchar#", "%c", 0 },
    { "zstr", "%s", 0 },
    { "udec", "%u", 0 },
    { "fdec", "%g", 0 },
    { "udec_byte", "%03u", 8 },
    { "udec_word", "%05u", 16 },
    { "udec_long", "%09u", 0 },
    { "sdec", "%d", 0 },
    { "sdec_byte", "%3d", -8 },
    { "sdec_word", "%5d", -16 },
    { "sdec_long", "%9d", 0 },
    { "uhex", "$%x", 0 },
    { "uhex_byte", "$%02x", 8 },
    { "uhex_word", "$%04x", 16 },
    { "uhex_long", "$%08x", 0 },
    { "dly", "%.0s", DBG_BITS_DELAY },
    { 0, 0, 0 }
};

static AST *GetFormatForDebug(struct flexbuf *fb, const char *itemname_orig, AST *args, int needcomma)
{
    char itemname[128];
    struct s_dbgfmt *ptr = &dbgfmt[0];
    AST *arg;
    AST *outlist = NULL;
    int len;
    int output_name = 1;
    const char *idname;
    int bits = 0;
    
    len = strlen(itemname_orig);
    if (len > sizeof(itemname)-1) {
        WARNING(args, "Unhandled debug format %s", itemname_orig);
        return NULL;
    }
    strcpy(itemname, itemname_orig);
    if (itemname[len-1] == '_') {
        output_name = 0;
        itemname[len-1] = 0;
    }
    while (ptr && ptr->name) {
        if (!strcasecmp(ptr->name, itemname)) {
            break;
        }
        ptr++;
    }
    if (!ptr->name) {
        WARNING(args, "Unhandled debug format %s", itemname_orig);
        return NULL;
    }
    bits = ptr->bits;
    if (bits == DBG_BITS_DELAY) {
        output_name = 0;
    }
    // now output all the arguments
    while (args) {
        arg = args->left;
        args = args->right;
    
        if (needcomma) {
            flexbuf_addstr(fb, ", ");
        }
        if (output_name) {
            idname = GetExprString(arg);
            flexbuf_printf(fb, "%s = %s", idname, ptr->cfmt);
        } else {
            flexbuf_addstr(fb, ptr->cfmt);
        }
        needcomma = 1;
        if (bits == DBG_BITS_DELAY) {
            // cause a delay
            arg = NewAST(AST_FUNCCALL, AstIdentifier("_waitms"),
                         NewAST(AST_EXPRLIST, arg, NULL));
        } else if (bits & 0x1f) {
            if (bits < 0) {
                arg = AstOperator(K_SIGNEXTEND, arg, AstInteger(-bits));
            } else {
                arg = AstOperator(K_ZEROEXTEND, arg, AstInteger(bits));
            }
        }
        arg = NewAST(AST_EXPRLIST, arg, NULL);
        outlist = AddToList(outlist, arg);
    }
    return outlist;
}

extern AST *genPrintf(AST *);

AST *
BuildDebugList(AST *exprlist)
{
    AST *outlist = NULL;
    AST *item;
    AST *printf_call;
    AST *sub;
    struct flexbuf fb;
    const char *fmtstr;
    int needcomma = 0;
    
    if (!gl_debug) {
        return NULL;
    }
    if (gl_brkdebug) {
        AST *debug = NewAST(AST_BRKDEBUG, exprlist, NULL);
        return debug;
    }
    flexbuf_init(&fb, 1024);

    if (exprlist && exprlist->left && exprlist->left->kind == AST_LABEL) {   
        flexbuf_addstr(&fb, "Cog%d  ");
        outlist = NewAST(AST_FUNCCALL,
                         AstIdentifier("cogid"),
                         NULL);
        outlist = NewAST(AST_EXPRLIST, outlist, NULL);
        exprlist = exprlist->right;
    } else {
        outlist = NULL;
    }
    while (exprlist && exprlist->kind == AST_EXPRLIST) {
        item = exprlist->left;
        exprlist = exprlist->right;
        if (!item) continue;
        if (item->kind == AST_STRING) {
            sub = NewAST(AST_STRINGPTR, item, NULL);
            sub = NewAST(AST_EXPRLIST, sub, NULL);
            outlist = AddToList(outlist, sub);
            flexbuf_addstr(&fb, "%s");
            needcomma=0;
        } else if (item->kind == AST_FUNCCALL) {
            const char *name = GetUserIdentifierName(item->left);
            AST *newarg;
            
            item = item->right; /* the parameter list */
            newarg = GetFormatForDebug(&fb, name, item, needcomma);
            if (newarg) {
                needcomma = 1;
                outlist = AddToList(outlist, newarg);
            }
        }
    }
    flexbuf_addstr(&fb, "\r\n");
    flexbuf_addchar(&fb, 0);
    fmtstr = flexbuf_get(&fb);
    flexbuf_delete(&fb);

    sub = AstStringPtr(fmtstr);
    sub = NewAST(AST_EXPRLIST, sub, NULL);
    outlist = AddToList(sub, outlist);
    printf_call = NewAST(AST_PRINT, NULL, NULL);
    printf_call = NewAST(AST_FUNCCALL, printf_call, outlist);
    return genPrintf(printf_call);
}

#define YYERROR_VERBOSE 1
%}

%token SP_IDENTIFIER "identifier"
%token SP_BYTECODE   "__bytecode__"
%token SP_NUM        "number"
%token SP_STRING     "string"
%token SP_BACKTICK_STRING "` string"
%token SP_FLOATNUM   "float point number"
%token SP_SPR        "SPR"
%token SP_COGREG     "REG"

/* various keywords */
%token SP_CON        "CON"
%token SP_VAR        "VAR"
%token SP_DAT        "DAT"
%token SP_PUB        "PUB"
%token SP_PRI        "PRI"
%token SP_OBJ        "OBJ"
%token SP_ASM        "ASM"
%token SP_ASM_CONST  "ASM_CONST"
%token SP_ENDASM     "ENDASM"
%token SP_END        "END"
%token SP_ASM_IF     "%IF"
%token SP_ASM_ELSEIF "%ELSEIF"
%token SP_ASM_ELSE   "%ELSE"
%token SP_ASM_ENDIF  "%ENDIF"
%token SP_INLINECCODE "CCODE"
%token SP_BYTE       "BYTE"
%token SP_WORD       "WORD"
%token SP_LONG       "LONG"
%token SP_BYTEFIT    "BYTEFIT"
%token SP_WORDFIT    "WORDFIT"
%token SP_FVAR       "FVAR"
%token SP_FVARS      "FVARS"
%token SP_ASMCLK     "ASMCLK"

%token SP_INSTR      "instruction"
%token SP_INSTRMODIFIER "instruction modifier"
%token SP_HWREG      "hardware register"
%token SP_ORG        "ORG"
%token SP_ORGH       "ORGH"
%token SP_ORGF       "ORGF"
%token SP_RES        "RES"
%token SP_FIT        "FIT"
%token SP_ALIGNL     "ALIGNL"
%token SP_ALIGNW     "ALIGNW"

%token SP_REPEAT     "REPEAT"
%token SP_FROM       "FROM"
%token SP_TO         "TO"
%token SP_STEP       "STEP"
%token SP_WHILE      "WHILE"
%token SP_UNTIL      "UNTIL"
%token SP_IF         "IF"
%token SP_IFNOT      "IFNOT"
%token SP_ELSE       "ELSE"
%token SP_ELSEIF     "ELSEIF"
%token SP_ELSEIFNOT  "ELSEIFNOT"
%token SP_THEN       "THEN"
%token SP_ENDIF      "ENDIF"

%token SP_LOOKDOWN   "LOOKDOWN"
%token SP_LOOKDOWNZ  "LOOKDOWNZ"
%token SP_LOOKUP     "LOOKUP"
%token SP_LOOKUPZ    "LOOKUPZ"
%token SP_COGINIT    "COGINIT"
%token SP_COGNEW     "COGNEW"
%token SP_PINR       "PINREAD"
%token SP_PINT       "PINTOGGLE"
%token SP_PINW       "PINWRITE"
%token SP_PINL       "PINLOW"
%token SP_PINH       "PINHIGH"

%token SP_CASE       "CASE"
%token SP_CASE_FAST  "CASE_FAST"
%token SP_OTHER      "OTHER"

%token SP_QUIT       "QUIT"
%token SP_NEXT       "NEXT"

%token SP_ALLOCA     "__BUILTIN_ALLOCA"
%token SP_REGEXEC    "REGEXEC"
%token SP_REGLOAD    "REGLOAD"

/* other stuff */
%token SP_ABORT      "ABORT"
%token SP_RESULT     "RESULT"
%token SP_RETURN     "RETURN"
%token SP_INDENT     "indentation"
%token SP_OUTDENT    "lack of indentation"
%token SP_EOLN       "end of line"
%token SP_EOF        "end of file"
%token SP_DOTS       ".."
%token SP_HERE       "$"
%token SP_STRINGPTR  "STRING"
%token SP_FILE       "FILE"

%token SP_ANNOTATION

/* operators */
%token SP_ASSIGN     ":="
%token SP_XOR        "XOR (^^)"
%token SP_OR         "OR (||)"
%token SP_AND        "AND (&&)"
%token SP_ORELSE     "__ORELSE__"
%token SP_ANDTHEN    "__ANDTHEN__"
%token SP_GE         "=>"
%token SP_LE         "=<"
%token SP_GEU        "+=>"
%token SP_LEU        "+=<"
%token SP_GTU        "+>"
%token SP_LTU        "+<"
%token SP_NE         "<>"
%token SP_EQ         "=="
%token SP_SGNCOMP    "<=>"
%token SP_LIMITMIN   "#>"
%token SP_LIMITMAX   "<#"
%token SP_REMAINDER  "//"
%token SP_UNSDIV     "+/"
%token SP_UNSMOD     "+//"
%token SP_FRAC       "FRAC"
%token SP_HIGHMULT   "**"
%token SP_SCAS        "SCAS"
%token SP_UNSHIGHMULT "SCA (+**)"
%token SP_ROTR       "ROR (->)"
%token SP_ROTL       "ROL (<-)"
%token SP_SHL        "<<"
%token SP_SHR        ">>"
%token SP_SAR        "SAR (~>)"
%token SP_REV        "><"
%token SP_REV2       "REV"
%token SP_ADDBITS    "ADDBITS"
%token SP_ADDPINS    "ADDPINS"
%token SP_NEGATE     "-"
%token SP_BIT_NOT    "!"
%token SP_SQRT       "SQRT (^^)"
%token SP_ABS        "ABS (||)"
%token SP_FSQRT      "FSQRT"
%token SP_FABS       "FABS"
%token SP_FIELD      "FIELD"
%token SP_DECODE     "DECOD (|<)"
%token SP_ENCODE     ">|"
%token SP_ENCODE2    "ENCOD"
%token SP_NOT        "NOT (!!)"
%token SP_DOUBLETILDE "~~"
%token SP_INCREMENT  "++"
%token SP_DECREMENT  "--"
%token SP_DOUBLEAT   "@@"
%token SP_TRIPLEAT   "@@@"
%token SP_FIELDPTR   "^@"
%token SP_FLOAT      "floating point number"
%token SP_TRUNC      "TRUNC"
%token SP_ROUND      "ROUND"
%token SP_CONSTANT   "constant"
%token SP_RANDOM     "??"
%token SP_EMPTY      "empty assignment marker _"
%token SP_SIGNX      "SIGNX"
%token SP_ZEROX      "ZEROX"
%token SP_NAN        "NAN"
%token SP_ONES       "ONES"
%token SP_BMASK      "BMASK"
%token SP_QLOG       "QLOG"
%token SP_QEXP       "QEXP"
%token SP_DEBUG      "DEBUG"
%token SP_LOOK_SEP   ": after lookup/down"
%token SP_CONDITIONAL  "?"
%token SP_CONDITIONAL_SEP ": after ?"

%token SP_FADD   "+."
%token SP_FSUB   "-."
%token SP_FMUL   "*."
%token SP_FDIV   "%."
%token SP_FLT    "<."
%token SP_FGT    ">."
%token SP_FNE    "<>."
%token SP_FEQ    "==."
%token SP_FLE    "<=."
%token SP_FGE    ">=."
%token SP_FNEGATE "float negate"

%token SP_DAT_LBRACK "[ in DAT"
%token SP_DAT_RBRACK "] in DAT"

/* operator precedence */
%right SP_ASSIGN
%left '\\'
%right SP_THEN
%right SP_ELSE
%left SP_CONDITIONAL    /* priority 16 */
%left SP_OR SP_ORELSE   /* priority 15 */
%left SP_XOR            /* priority 14 */
%left SP_AND SP_ANDTHEN /* priority 13 */
%left SP_NOT /* priority 12 */
%left '<' '>' SP_GE SP_LE SP_NE SP_EQ SP_SGNCOMP SP_GEU SP_LEU SP_GTU SP_LTU SP_FGT SP_FLT SP_FGE SP_FLE SP_FNE SP_FEQ /*priority 11 */
%left SP_ADDBITS SP_ADDPINS   /* priority 10 */
%left SP_LIMITMIN SP_LIMITMAX /* priority 9 */
%left '-' '+' SP_FADD SP_FSUB /* priority 8 */
%left '*' '/' SP_FMUL SP_FDIV SP_REMAINDER SP_HIGHMULT SP_UNSHIGHMULT SP_SCAS SP_UNSDIV SP_UNSMOD SP_FRAC /* priority 7 */
%left '|' /* priority 6 */
%left '^' /* priority 5 */
%left '&' /* priority 4 */
%left SP_ROTL SP_ROTR SP_SHL SP_SHR SP_SAR SP_REV SP_REV2 SP_SIGNX SP_ZEROX /* priority 3 */
%left SP_NEGATE SP_FNEGATE SP_BIT_NOT SP_ABS SP_FABS SP_SQRT SP_FSQRT SP_DECODE SP_ENCODE SP_ENCODE2 SP_ALLOCA SP_ONES SP_BMASK SP_QLOG SP_QEXP /* priority 2 in Spin2 */
%left '@' '~' '?' SP_RANDOM SP_DOUBLETILDE SP_INCREMENT SP_DECREMENT SP_DOUBLEAT SP_TRIPLEAT  SP_FIELDPTR /* priority 1 in Spin2 */
%left SP_CONSTANT SP_FLOAT SP_TRUNC SP_ROUND SP_NAN

%%
input:
  rest
  | conblock rest
    {
        if (current) {
            current->conblock = AddToListEx(current->conblock, $1, &current->conblock_tail);
        }
    }
;

rest:
  topelement
  | topelement rest
  ;

emptyline:
  SP_EOLN
  ;

emptylines: 
  | emptylines emptyline
  ;

topelement:
  SP_CON conblock
  { $$ = current->conblock = AddToListEx(current->conblock, $2, &current->conblock_tail); }
  | SP_DAT datblock
  { $$ = current->datblock = AddToListEx(current->datblock, $2, &current->datblock_tail); }
  | SP_DAT annotation datblock
  {
      current->datannotations = AddToList(current->datannotations, $2);
      $$ = current->datblock = AddToListEx(current->datblock, $3, &current->datblock_tail); 
  }
  | SP_VAR varblock
  { $$ = current->pendingvarblock = AddToList(current->pendingvarblock, $2); }
  | SP_OBJ objblock
  {
    $$ = current->objblock = AddToList(current->objblock, $2);
  }
  | SP_PUB funcdef funcbody
    { DeclareFunction(current, SpinRetType($2), 1, $2, $3, NULL, $1); }
  | SP_PRI funcdef funcbody
    { DeclareFunction(current, SpinRetType($2), 0, $2, $3, NULL, $1); }
  | SP_PUB annotation funcdef funcbody
    { DeclareFunction(current, SpinRetType($3), 1, $3, $4, $2, $1); }
  | SP_PRI annotation funcdef funcbody
    { DeclareFunction(current, SpinRetType($3), 0, $3, $4, $2, $1); }
  | SP_PUB SP_FILE SP_STRING funcdef
    { DeclareFunction(current, SpinRetType($4), 1, $4, $3, NULL, $1); }
  | SP_PRI SP_FILE SP_STRING funcdef
    { DeclareFunction(current, SpinRetType($4), 0, $4, $3, NULL, $1); }

  | SP_PUB annotation SP_FILE SP_STRING funcdef
    { DeclareFunction(current, SpinRetType($5), 1, $5, $4, $2, $1); }
  | SP_PRI annotation SP_FILE SP_STRING funcdef
    { DeclareFunction(current, SpinRetType($5), 0, $5, $4, $2, $1); }

  | SP_PRI identifier '=' identifier SP_EOLN
    {
        const char *oldname = GetIdentifierName($4);
        const char *newname = GetIdentifierName($2);
        AddSymbol(&current->objsyms, newname, SYM_WEAK_ALIAS, (void *)oldname, NULL);
    }
  | SP_PUB identifier '=' identifier SP_EOLN
    {
        const char *oldname = GetIdentifierName($4);
        const char *newname = GetIdentifierName($2);
        AddSymbol(&current->objsyms, newname, SYM_WEAK_ALIAS, (void *)oldname, NULL);
    }
  | annotation emptylines
    { DeclareToplevelAnnotation($1); }
;

funcdef:
  identifier optparamlist resultname localvars SP_EOLN
  { AST *funcdecl = NewAST(AST_FUNCDECL, $1, $3);
    AST *funcvars = NewAST(AST_FUNCVARS, $2, $4);
    $$ = NewAST(AST_FUNCDEF, funcdecl, funcvars);
  }
;

optparamlist:
/* empty */
  {
      $$ = NULL;
      LANGUAGE_WARNING(LANG_SPIN_SPIN2, NULL, "omitting () for empty parameter lists is a flexspin extension");
  }
| '(' ')'
  {
      $$ = NULL;
      LANGUAGE_WARNING(LANG_SPIN_SPIN1, NULL, "() for empty parameter lists is a flexspin extension");
  }
| paramidentlist
  { $$ = $1; }
| '(' paramidentlist ')'
  { $$ = $2; }
  ;

resultname:
/* empty */
  { $$ = NULL; }
| ':' paramidentlist
  {
      // handle the common case of just one identifier by
      // unwrapping the list
      AST *list = $2;
      if (list->kind == AST_LISTHOLDER && list->right == NULL) {
          list = list->left;
      }
      $$ = list;
  }
  ;

localvars:
/* empty */
  { $$ = NULL; }
| '|' vardecllist
  { $$ = $2; }
    ;

funcbody:
  /* empty */
  { $$ = NULL; }
| SP_BYTECODE '(' SP_STRING ')' SP_EOLN
  {
      AST *str = $3;
      str->kind = AST_BYTECODE;
      $$ = str;
  }
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
  | SP_EOLN
    { $$ = NULL; }
  | error SP_EOLN
    { $$ = NULL; }
  ;

basicstmt:
   SP_RETURN SP_EOLN
    { $$ = AstReturn(NULL, $1); }
  |  SP_RETURN '(' exprlist ')' SP_EOLN
    {
        $$ = AstReturn($3, $1);
    }
  |  SP_RETURN exprlist SP_EOLN
    { $$ = AstReturn($2, $1); }
  | SP_ABORT SP_EOLN
    {
        if (current && current->curLanguage == LANG_SPIN_SPIN2) {
            // in Spin2 plain "abort" is the same as "abort 0"
            $$ = AstAbort(AstInteger(0), $1);
        } else {
            // in Spin1 plain "abort" should return the result variable
            $$ = AstAbort(NULL, $1);
        }
    }
  |  SP_ABORT expr SP_EOLN
    { $$ = AstAbort($2, $1); }
  | multiassign
  | expr SP_EOLN
    { $$ = $1; }
  | SP_QUIT SP_EOLN
    { $$ = NewCommentedAST(AST_QUITLOOP, NULL, NULL, $1); }
  | SP_NEXT SP_EOLN
    { $$ = NewCommentedAST(AST_CONTINUE, NULL, NULL, $1); }
  | SP_DEBUG '(' ')' SP_EOLN
    {
        AST *ast = NULL;
        AST *comment = $1;
        if (comment) {
            ast = NewAST(AST_COMMENTEDNODE, ast, comment);
        }
        $$ = ast;
    }
  | SP_DEBUG '(' debug_exprlist ')' SP_EOLN
    {
        AST *ast = BuildDebugList($3);
        AST *comment = $1;
        if (comment) {
            ast = NewAST(AST_COMMENTEDNODE, ast, comment);
        }
        $$ = ast;
    }
;

debug_exprlist:
  debug_expritem_first
     { $$ = $1; }
  | debug_expritem_first ',' debug_exprlist_continue
     { $$ = AddToList($1, $3); }
;

debug_exprlist_continue:
   debug_expritem
     { $$ = $1; }
   | debug_exprlist_continue ',' debug_expritem
     { $$ = AddToList($1, $3); }
   ;

debug_expritem_first:
  SP_BACKTICK_STRING
    {
        $$ = NewAST(AST_EXPRLIST, $1, NULL);
    }
  | expritem
    {
        AST *list = $1;
        AST *note = NewAST(AST_LABEL, NULL, NULL);

        $$ = NewAST(AST_EXPRLIST, note, list);
    }
  ;
debug_expritem:
  SP_BACKTICK_STRING
    { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  | expritem
    { $$ = $1; }
  ;

asmdebug_func:
  identifier '(' operandlist ')'
    { $$ = MakeFunccall($1,$3,NULL);}
  | SP_IF '(' operandlist ')'
    { $$ = MakeFunccall(AstIdentifier("if"),$3,NULL);}
  | SP_IFNOT '(' operandlist ')'
    { $$ = MakeFunccall(AstIdentifier("ifnot"),$3,NULL);}
;

asmdebug_exprlist:
  asmdebug_expritem_first
  | asmdebug_expritem_first ',' asmdebug_exprlist_continue
     { $$ = AddToList($1, $3); }
;

asmdebug_exprlist_continue:
   asmdebug_expritem
   | asmdebug_exprlist_continue ',' asmdebug_expritem
     { $$ = AddToList($1, $3); }
   ;

asmdebug_expritem_first:
/*  SP_BACKTICK_STRING
    {
        $$ = NewAST(AST_EXPRLIST, $1, NULL);
    }
    | */
asmdebug_expritem
    {
        AST *list = $1;
        AST *note = NewAST(AST_LABEL, NULL, NULL);

        $$ = NewAST(AST_EXPRLIST, note, list);
    }
  ;

asmdebug_expritem:
  SP_BACKTICK_STRING
    { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  | string
    { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  | integer // Becomes a char...
    { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  | asmdebug_func
    { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  ;

multiassign:
  lhsseq SP_ASSIGN '(' exprlist ')'
    { $$ = AstAssignList($1, $4, $2); }
  | lhsseq SP_ASSIGN exprlist
    { $$ = AstAssignList($1, $3, $2); }

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
  SP_INDENT stmtlist SP_OUTDENT
  { $$ = $2; }
  | SP_INDENT SP_OUTDENT
  { $$ = NULL; }
;

ifstmt:
  SP_IF expr SP_EOLN elseblock
    { $$ = NewCommentedAST(AST_IF, $2, $4, $1); }
  | SP_IFNOT expr SP_EOLN elseblock
    { $$ = NewCommentedAST(AST_IF, AstOperator(K_BOOL_NOT, NULL, $2), $4, $1); }
;

elseblock:
  stmtblock
    { $$ = NewAST(AST_THENELSE, $1, NULL); }
  | stmtblock SP_ELSE SP_EOLN stmtblock
  { $$ = NewCommentedAST(AST_THENELSE, $1, $4, $2); }
  | stmtblock SP_ELSEIF expr SP_EOLN elseblock
    { $$ = NewAST(AST_THENELSE, $1, NewAST(AST_STMTLIST, NewCommentedAST(AST_IF, $3, $5, $2), NULL)); }
  | stmtblock SP_ELSEIFNOT expr SP_EOLN elseblock
    { $$ = NewAST(AST_THENELSE, $1, NewAST(AST_STMTLIST, NewCommentedAST(AST_IF, AstOperator(K_BOOL_NOT, NULL, $3), $5, $2), NULL)); }
  ;

casestmt:
  SP_CASE expr SP_EOLN SP_INDENT casematchlist SP_OUTDENT
    { $$ = NewCommentedAST(AST_CASE, $2, $5, $1); }
  | SP_CASE_FAST expr SP_EOLN SP_INDENT casematchlist SP_OUTDENT
    { $$ = NewCommentedAST(AST_CASETABLE, $2, $5, $1); }
;

casematchlist:
  casematchitem
    { $$ = $1; }
  | casematchlist casematchitem
    { $$ = AddToList($1, $2); }
  ;

casematchitem:
  casematch SP_EOLN stmtblock
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
  matchexprlist ':'
  {
      $$ = $1;
      EstablishIndent(current->Lptr, -1);
      resetLineState(current->Lptr);
  }

matchexprlist:
  matchexpritem
  | matchexprlist ',' matchexpritem
    { $$ = AddToList($1, $3); }
  ;

matchexpritem:
  SP_OTHER
    { $$ = NewAST(AST_OTHER, NULL, NULL); }
  | expr SP_DOTS expr
    { $$ = NewAST(AST_EXPRLIST, NewAST(AST_RANGE, $1, $3), NULL); }
  | expr
    { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  ;


rangeexpritem:
  expr
    { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  | expr SP_DOTS expr
    { $$ = NewAST(AST_EXPRLIST, NewAST(AST_RANGE, $1, $3), NULL); }
  ;

rangeexprlist:
  rangeexpritem
  | rangeexprlist ',' rangeexpritem
    { $$ = AddToList($1, $3); }
  ;

repeatstmt:
    SP_REPEAT SP_EOLN stmtblock
    {   AST *body = $3; body = CheckYield(body);
        AST *one = AstInteger(1);
        one->lineidx = $1->lineidx;
        $$ = NewCommentedAST(AST_WHILE, one, body, $1);
        $$->lineidx = $1->lineidx;
    }
  | SP_REPEAT SP_EOLN stmtblock SP_WHILE expr SP_EOLN
    { $$ = NewCommentedAST(AST_DOWHILE, $5, CheckYield($3), $1); }
  | SP_REPEAT SP_EOLN stmtblock SP_UNTIL expr SP_EOLN
    { $$ = NewCommentedAST(AST_DOWHILE, AstOperator(K_BOOL_NOT, NULL, $5), CheckYield($3), $1); }
  | SP_REPEAT SP_WHILE expr SP_EOLN stmtblock
    {   AST *body = $5; body = CheckYield(body); 
        $$ = NewCommentedAST(AST_WHILE, $3, body, $1);
        $$->lineidx = $1->lineidx;
    }
  | SP_REPEAT SP_UNTIL expr SP_EOLN stmtblock
    {   AST *body = $5;
        AST *expr = AstOperator(K_BOOL_NOT, NULL, $3);
        expr->lineidx = $3->lineidx;
        body = CheckYield(body); 
        $$ = NewCommentedAST(AST_WHILE, expr, body, $1);
        $$->lineidx = $1->lineidx;
    }
  | SP_REPEAT identifier SP_FROM expr SP_TO expr SP_STEP expr SP_EOLN stmtblock
    {
      AST *from, *to, *step; 
      step = NewAST(AST_STEP, $8, $10);
      to = NewAST(AST_TO, $6, step);
      from = NewAST(AST_FROM, $4, to);
      $$ = NewCommentedAST(AST_COUNTREPEAT, $2, from, $1);
    }
  | SP_REPEAT identifier SP_FROM expr SP_TO expr SP_EOLN stmtblock
    {
      AST *from, *to, *step; 
      step = NewAST(AST_STEP, AstInteger(1), $8);
      to = NewAST(AST_TO, $6, step);
      from = NewAST(AST_FROM, $4, to);
      $$ = NewCommentedAST(AST_COUNTREPEAT, $2, from, $1);
    }
  | SP_REPEAT expr SP_EOLN stmtblock
    {
      AST *from, *to, *step;
      AST *body = $4;
      body = CheckYield(body);
      step = NewAST(AST_STEP, AstInteger(1), body);
      to = NewAST(AST_TO, $2, step);
      from = NewAST(AST_FROM, NULL, to);
      $$ = NewCommentedAST(AST_COUNTREPEAT, NULL, from, $1);
    }
  | SP_ASM datblock SP_ENDASM
    {
        AST *ast = NewCommentedAST(AST_INLINEASM, $2, AstInteger(0), $1);
        $$ = ast;
        LANGUAGE_WARNING(LANG_ANY, NULL, "asm/endasm is a flexspin extension");
    }
  | SP_ASM_CONST datblock SP_ENDASM
    {
        AST *ast = NewCommentedAST(AST_INLINEASM, $2, AstInteger(1), $1);
        $$ = ast;
        LANGUAGE_WARNING(LANG_ANY, NULL, "asm/endasm is a flexspin extension");
    }
  | SP_ORGH datblock SP_END
    {
        // flag 1 means const (do not optimize) & no FCACHE
        AST *ast = NewCommentedAST(AST_INLINEASM, $2, AstInteger(1), $1);
        $$ = ast;
    }
  | SP_ORG datblock SP_END
    {
        // flag 3 means const (do not optimize) & FCACHE
        AST *ast = NewCommentedAST(AST_INLINEASM, $2, AstInteger(3), $1);
        $$ = ast;
    }
  | SP_INLINECCODE
    {  $$ = $1; }
;

lookupexpr:
SP_LOOKUPZ '(' expr SP_LOOK_SEP rangeexprlist ')'
    { $$ = AstLookup(AST_LOOKUP, 0, $3, $5); }
  | SP_LOOKUP '(' expr SP_LOOK_SEP rangeexprlist ')'
    { $$ = AstLookup(AST_LOOKUP, 1, $3, $5); }
;
lookdownexpr:
  SP_LOOKDOWNZ '(' expr SP_LOOK_SEP rangeexprlist ')'
    { $$ = AstLookup(AST_LOOKDOWN, 0, $3, $5); }
  | SP_LOOKDOWN '(' expr SP_LOOK_SEP rangeexprlist ')'
    { $$ = AstLookup(AST_LOOKDOWN, 1, $3, $5); }
;

conblock:
  conline
  { $$ = $1; }
  | conblock conline
  { $$ = AddToList($1, $2); }
  ;

conline:
  enumlist SP_EOLN
    { $$ = $1; }
  | SP_EOLN
    { $$ = NULL; }
  | error SP_EOLN
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
  | '#' expr '[' expr ']'
    { $$ = NewAST(AST_ENUMSET, $2, $4); }
  ;

datblock:
  datline
    {
        AST *dat = $1;
        $$ = dat; //NewAST(AST_LISTHOLDER, dat, NULL);
        current->parse_tail = NULL;
    }
  | datblock datline
    { $$ = AddToListEx($1, $2, &current->parse_tail); }
  ;

datline:
  basedatline
  | identifier basedatline
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
  SP_EOLN
    { $$ = NULL; }
  | error SP_EOLN
    { $$ = NULL; }
  | SP_BYTE SP_EOLN
    { $$ = NewCommentedAST(AST_BYTELIST, NULL, NULL, $1); }
  | SP_BYTE datexprlist SP_EOLN
    { $$ = NewCommentedAST(AST_BYTELIST, FixupList($2), NULL, $1); }
  | SP_WORD SP_EOLN
    { $$ = NewCommentedAST(AST_WORDLIST, NULL, NULL, $1); }
  | SP_WORD datexprlist SP_EOLN
    { $$ = NewCommentedAST(AST_WORDLIST, FixupList($2), NULL, $1); }
  | SP_BYTEFIT SP_EOLN
    { $$ = NewCommentedAST(AST_BYTELIST, NULL, NULL, $1); }
  | SP_BYTEFIT datexprlist SP_EOLN
    {
        $$ = NewCommentedAST(AST_BYTEFITLIST, FixupList($2), NULL, $1);
    }
  | SP_WORDFIT SP_EOLN
    {
        $$ = NewCommentedAST(AST_WORDLIST, NULL, NULL, $1);
    }
  | SP_WORDFIT datexprlist SP_EOLN
    { $$ = NewCommentedAST(AST_WORDFITLIST, FixupList($2), NULL, $1); }
  | SP_LONG SP_EOLN
    { $$ = NewCommentedAST(AST_LONGLIST, NULL, NULL, $1); }
  | SP_LONG datexprlist SP_EOLN
    { $$ = NewCommentedAST(AST_LONGLIST, FixupList($2), NULL, $1); }
  | instruction SP_EOLN
    { $$ = NewCommentedInstr($1); }
  | instruction operandlist SP_EOLN
    { $$ = NewCommentedInstr(AddToList($1, $2)); }
  | instruction modifierlist SP_EOLN
    { $$ = NewCommentedInstr(AddToList($1, $2)); }
  | instruction operandlist modifierlist SP_EOLN
    { $$ = NewCommentedInstr(AddToList($1, AddToList($2, $3))); }
  | SP_ASM_IF expr SP_EOLN
    {
        $$ = NewCommentedAST(AST_ASM_IF, $2, NULL, $1);
    }  
  | SP_ASM_ELSEIF expr SP_EOLN
    {
        $$ = NewCommentedAST(AST_ASM_ELSEIF, $2, NULL, $1);
    }  
  | SP_ASM_ELSE SP_EOLN
    {
        $$ = NewCommentedAST(AST_ASM_ELSEIF, AstInteger(1), NULL, $1);
    }  
  | SP_ASM_ENDIF SP_EOLN
    {
        $$ = NewCommentedAST(AST_ASM_ENDIF, NULL, NULL, $1);
    }  
  | SP_ALIGNL SP_EOLN
    { $$ = NewCommentedAST(AST_ALIGN, AstInteger(4), NULL, $1); }
  | SP_ALIGNW SP_EOLN
    { $$ = NewCommentedAST(AST_ALIGN, AstInteger(2), NULL, $1); }
  | SP_ORG SP_EOLN
    { $$ = NewCommentedAST(AST_ORG, NULL, NULL, $1); }
  | SP_ORG expr SP_EOLN
    { $$ = NewCommentedAST(AST_ORG, $2, NULL, $1); }
  | SP_ORG expr ',' expr SP_EOLN
    {
        AST *src = $2;
        AST *limit = $4;
        LANGUAGE_WARNING(LANG_SPIN_SPIN1, NULL, "second parameter for ORG is only valid in Spin2");
        src = NewAST(AST_RANGE, src, limit);
        $$ = NewCommentedAST(AST_ORG, src, NULL, $1);
    }
  | SP_ORGH SP_EOLN
    { $$ = NewCommentedAST(AST_ORGH, NULL, NULL, $1); }
  | SP_ORGH expr SP_EOLN
    { $$ = NewCommentedAST(AST_ORGH, $2, NULL, $1); }
  | SP_ORGH expr ',' expr SP_EOLN
    {
        AST *addr = NewAST(AST_RANGE, $2, $4);
        $$ = NewCommentedAST(AST_ORGH, addr, NULL, $1);
    }
  | SP_ORGF expr SP_EOLN
    { $$ = NewCommentedAST(AST_ORGF, $2, NULL, $1); }
  | SP_RES expr SP_EOLN
    { $$ = NewCommentedAST(AST_RES, $2, NULL, $1); }
  | SP_FIT expr SP_EOLN
    { $$ = NewCommentedAST(AST_FIT, $2, NULL, $1); }
  | SP_FIT SP_EOLN
    { $$ = NewCommentedAST(AST_FIT, AstInteger(0x1f0), NULL, $1); }
  | SP_FILE string SP_EOLN
    { $$ = NewCommentedAST(AST_FILE, GetFullFileName($2), NULL, $1); }
  | SP_DEBUG '(' ')' SP_EOLN
    {
        if (!gl_brkdebug) {
          // for now just ignore DEBUG in PASM
          $$ = NULL;
        } else {
          // TODO: What does empty debug do??
          $$ = NULL;
        }
    }
  | SP_DEBUG '(' asmdebug_exprlist ')' SP_EOLN
    {
        AST *ast = NULL;
        if (gl_brkdebug) {
          ast = NewAST(AST_BRKDEBUG,$3,NULL);
        }
        if ($1) {
            ast = NewAST(AST_COMMENTEDNODE, ast, $1);
        }
        $$ = ast;
    }
  | SP_ASMCLK
  {
      SYNTAX_ERROR("ASMCLK instruction is not supported");
  }
  ;

objblock:
  objline
  { $$ = $1; }
  | objblock objline
  { $$ = AddToList($1, $2); }
;

objline:
    SP_EOLN
    { $$ = NULL; }
  | error  SP_EOLN
    { $$ = NULL; }
  | identdecl ':' string optobjparams
    {
        AST *paramlist = $4;
        AST *filename = $3;
        AST *ident = $1;
        AST *typ = NewObjectWithParams(ident, filename, 1, paramlist);
        /* last parameter is 1 for a private member */
        DeclareOneMemberVar(current, ident, typ, 0);
        $$ = typ;
    }
  | SP_EMPTY ':' string optobjparams
    {
        AST *paramlist = $4;
        AST *filename = $3;
        AST *ident = AstTempIdentifier("__anonymous__");
        
        AST *typ = NewObjectWithParams(ident, filename, 1, paramlist);
        Module *P;
        
        /* last parameter is 1 for a private member */
        DeclareOneMemberVar(current, ident, typ, 0);

        /* create aliases */
        P = GetClassPtr(typ);
        if (!P) {
            SYNTAX_ERROR("Internal error, could not find class for _ declaration");
        } else {
            DeclareAnonymousAliases(current, P, ident);
        }
        $$ = typ;
    }
  | identdecl '=' string optobjparams
    {
        AST *paramlist = $4;
        AST *filename = $3;
        AST *typ = NewAbstractObjectWithParams($1, filename, 1, paramlist);
        AST *ident = $1;
        const char *name = GetIdentifierName(ident);
        AddSymbol(&current->objsyms, name, SYM_TYPEDEF, typ, NULL);
        $$ = typ;
    }
;

optobjparams:
    /* empty */
      { $$ = NULL; }
  | '|' conline
      { $$ = $2; }
;

varblock:
    varline
    { $$ = CommentedListHolder($1); }
  | varblock varline
    { $$ = AddToList($1, CommentedListHolder($2)); }
  ;

varline:
  SP_BYTE identlist SP_EOLN
    { $$ = NewAST(AST_BYTELIST, $2, NULL); }
  | SP_WORD identlist SP_EOLN
    { $$ = NewAST(AST_WORDLIST, $2, NULL); }
  | SP_LONG identlist SP_EOLN
    { $$ = NewAST(AST_LONGLIST, $2, NULL); }
  | identdecl SP_EOLN
    {
        AST *decl = NewAST(AST_LISTHOLDER, $1, NULL);
        $$ = NewAST(AST_LONGLIST, decl, NULL);
    }
  | SP_ALIGNL SP_EOLN
    { $$ = NewCommentedAST(AST_ALIGN, AstInteger(4), NULL, $1); }
  | SP_ALIGNW SP_EOLN
    { $$ = NewCommentedAST(AST_ALIGN, AstInteger(2), NULL, $1); }
  | SP_EOLN
    { $$ = NULL; }
  | error SP_EOLN
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

vardecl:
  identdecl
    { $$ = $1; }
  | SP_BYTE identdecl
    { $$ = NewAST(AST_DECLARE_VAR, ast_type_byte, $2); }
  | SP_WORD identdecl
    { $$ = NewAST(AST_DECLARE_VAR, ast_type_word, $2); }
  | SP_LONG identdecl
    { $$ = NewAST(AST_DECLARE_VAR, NULL, $2); }
;

vardecllist:
   vardecl
      { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
   | vardecllist ',' vardecl
      { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $3, NULL)); }
   ;

paramidentdecl:
  identifier
  { $$ = $1; }
  | identifier '[' expr ']'
  { $$ = NewAST(AST_ARRAYDECL, $1, $3); }
  | identifier '=' expr
  {
      LANGUAGE_WARNING(LANG_ANY, $1, "default parameter values are a flexspin extension");
      $$ = AstAssign($1, $3);
  }
  | identifier '=' SP_LONG
  {
      LANGUAGE_WARNING(LANG_ANY, $1, "parameter types are a flexspin extension");
      $$ = NewAST(AST_DECLARE_VAR, ast_type_long, $1);
  }
  | identifier '=' '-' SP_LONG
  {
      LANGUAGE_WARNING(LANG_ANY, $1, "parameter types are a flexspin extension");
      $$ = NewAST(AST_DECLARE_VAR, ast_type_long, $1);
  }
  | identifier '=' '+' SP_LONG
  {
      LANGUAGE_WARNING(LANG_ANY, $1, "parameter types are a flexspin extension");
      $$ = NewAST(AST_DECLARE_VAR, ast_type_unsigned_long, $1);
  }
  | identifier '=' '>' SP_LONG
  {
      LANGUAGE_WARNING(LANG_ANY, $1, "parameter types are a flexspin extension");
      $$ = NewAST(AST_DECLARE_VAR, ast_type_long64, $1);
  }
  | identifier '=' '>' '+' SP_LONG
  {
      LANGUAGE_WARNING(LANG_ANY, $1, "parameter types are a flexspin extension");
      $$ = NewAST(AST_DECLARE_VAR, ast_type_unsigned_long64, $1);
  }
  | identifier '=' SP_FLOAT
  {
      LANGUAGE_WARNING(LANG_ANY, $1, "parameter types are a flexspin extension");
      $$ = NewAST(AST_DECLARE_VAR, ast_type_float, $1);
  }
  | identifier '=' '@' SP_LONG
  {
      LANGUAGE_WARNING(LANG_ANY, $1, "parameter types are a flexspin extension");
      $$ = NewAST(AST_DECLARE_VAR, ast_type_ptr_long, $1);
  }
  | identifier '=' '@' SP_WORD
  {
      LANGUAGE_WARNING(LANG_ANY, $1, "parameter types are a flexspin extension");
      $$ = NewAST(AST_DECLARE_VAR, ast_type_ptr_word, $1);
  }
  | identifier '=' '@' SP_BYTE
  {
      LANGUAGE_WARNING(LANG_ANY, $1, "parameter types are a flexspin extension");
      $$ = NewAST(AST_DECLARE_VAR, ast_type_ptr_byte, $1);
  }
  | identifier '=' SP_STRINGPTR
  {
      LANGUAGE_WARNING(LANG_ANY, $1, "parameter types are a flexspin extension");
      $$ = NewAST(AST_DECLARE_VAR, ast_type_string, $1);
  }
  | identifier '=' SP_TRIPLEAT
  {
      AST *anyref = NewAST(AST_REFTYPE, ast_type_generic, NULL);
      LANGUAGE_WARNING(LANG_ANY, $1, "parameter types are a flexspin extension");
      $$ = NewAST(AST_DECLARE_VAR, anyref, $1);
  }
  ;

paramidentlist:
  paramidentdecl
  { $$ = NewAST(AST_LISTHOLDER, $1, NULL); }
  | annotation paramidentdecl
  { $$ = AddToList(NewAST(AST_LISTHOLDER, $1, NULL),
                   NewAST(AST_LISTHOLDER, $2, NULL)); }
  | paramidentlist ',' paramidentdecl
  { $$ = AddToList($1, NewAST(AST_LISTHOLDER, $3, NULL)); }
  ;

expr:
  integer
  | float
  | string
  | SP_STRINGPTR '(' exprlist ')'
    {
        AST *elist = $3;
        $$ = NewAST(AST_STRINGPTR, elist, NULL);
    }
  | lhs
  | '@' expr
    {
        AST *e = $2;
        if (e && e->kind == AST_STRING) {
            LANGUAGE_WARNING(LANG_SPIN_SPIN1, NULL, "@\"string\" is a flexspin extension to Spin1");
            $$ = NewAST(AST_STRINGPTR,
                        NewAST(AST_EXPRLIST, e, NULL),
                        NULL);
        } else {
            $$ = NewAST(AST_ADDROF, e, NULL);
        }
    }
  | SP_DOUBLEAT expr
    { $$ = NewAST(AST_DATADDROF, $2, NULL); }
  | SP_TRIPLEAT expr
    { $$ = NewAST(AST_ABSADDROF, $2, NULL); }
  | SP_FIELDPTR expr
    {
        AST *ref = $2;
        $$ = NewAST(AST_FIELDADDR, ref, NULL); // placeholder
    }
  | lhs SP_ASSIGN expr
    { $$ = AstAssign($1, $3); }
  | identifier '#' identifier
    { $$ = NewAST(AST_CONSTREF, $1, $3); }
  | lhs '\\' expr
    { $$ = NewAST(AST_POSTSET, $1, $3); }
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
  | expr SP_GE expr
    { $$ = AstOperator(K_GE, $1, $3); }
  | expr SP_LE expr
    { $$ = AstOperator(K_LE, $1, $3); }
  | expr SP_GEU expr
    { $$ = AstOperator(K_GEU, $1, $3); }
  | expr SP_LEU expr
    { $$ = AstOperator(K_LEU, $1, $3); }
  | expr SP_GTU expr
    { $$ = AstOperator(K_GTU, $1, $3); }
  | expr SP_LTU expr
    { $$ = AstOperator(K_LTU, $1, $3); }
  | expr SP_NE expr
    { $$ = AstOperator(K_NE, $1, $3); }
  | expr SP_EQ expr
    { $$ = AstOperator(K_EQ, $1, $3); }
  | expr SP_SGNCOMP expr
    { $$ = AstOperator(K_SGNCOMP, $1, $3); }
  | expr SP_FEQ expr
    { $$ = AstOperator(K_FEQ, $1, $3); }
  | expr SP_FNE expr
    { $$ = AstOperator(K_FNE, $1, $3); }
  | expr SP_FGT expr
    { $$ = AstOperator(K_FGT, $1, $3); }
  | expr SP_FLT expr
    { $$ = AstOperator(K_FLT, $1, $3); }
  | expr SP_FGE expr
    { $$ = AstOperator(K_FGE, $1, $3); }
  | expr SP_FLE expr
    { $$ = AstOperator(K_FLE, $1, $3); }
  | expr '<' SP_GE expr
    // lexer quirk, <=> gets parsed as <  => in Spin1
    { $$ = AstOperator(K_SGNCOMP, $1, $4); }
  | expr SP_REMAINDER expr
    { $$ = AstOperator(K_MODULUS, $1, $3); }
  | expr SP_UNSDIV expr
    { $$ = AstOperator(K_UNS_DIV, $1, $3); }
  | expr SP_UNSMOD expr
    { $$ = AstOperator(K_UNS_MOD, $1, $3); }
  | expr SP_HIGHMULT expr
    { $$ = AstOperator(K_HIGHMULT, $1, $3); }
  | expr SP_SCAS expr
    { $$ = AstOperator(K_SCAS, $1, $3); }
  | expr SP_UNSHIGHMULT expr
    { $$ = AstOperator(K_UNS_HIGHMULT, $1, $3); }
  | expr SP_FRAC expr
    { $$ = AstOperator(K_FRAC64, $1, $3); }
  | expr SP_LIMITMIN expr
    { $$ = AstOperator(K_LIMITMIN, $1, $3); }
  | expr SP_LIMITMAX expr
    { $$ = AstOperator(K_LIMITMAX, $1, $3); }
  | expr SP_ZEROX expr
    { $$ = AstOperator(K_ZEROEXTEND, $1, AstOperator('+', $3, AstInteger(1))); }
  | expr SP_SIGNX expr
    { $$ = AstOperator(K_SIGNEXTEND, $1, AstOperator('+', $3, AstInteger(1))); }
  | expr SP_REV expr
    { $$ = AstOperator(K_REV, $1, $3); }
  | expr SP_REV2 expr
    { $$ = AstOperator(K_REV, $1, AstOperator('+', $3, AstInteger(1))); }
  | expr SP_ADDBITS expr
    { $$ = AstOperator('|', $1, AstOperator(K_SHL, $3, AstInteger(5))); }
  | expr SP_ADDPINS expr
    { $$ = AstOperator('|', $1, AstOperator(K_SHL, $3, AstInteger(6))); }
  | expr SP_ROTL expr
    { $$ = AstOperator(K_ROTL, $1, $3); }
  | expr SP_ROTR expr
    { $$ = AstOperator(K_ROTR, $1, $3); }
  | expr SP_SHL expr
    { $$ = AstOperator(K_SHL, $1, $3); }
  | expr SP_SHR expr
    { $$ = AstOperator(K_SHR, $1, $3); }
  | expr SP_SAR expr
    { $$ = AstOperator(K_SAR, $1, $3); }
  | expr SP_ANDTHEN expr
    { $$ = AstOperator(K_BOOL_AND, $1, $3); }
  | expr SP_ORELSE expr
    { $$ = AstOperator(K_BOOL_OR, $1, $3); }
  | expr SP_AND expr
    { $$ = AstOperator(K_LOGIC_AND, $1, $3); }
  | expr SP_OR expr
    { $$ = AstOperator(K_LOGIC_OR, $1, $3); }
  | expr SP_XOR expr
    { $$ = AstOperator(K_LOGIC_XOR, $1, $3); }
  | expr SP_FADD expr
    { $$ = AstOperator(K_FADD, $1, $3); }
  | expr SP_FSUB expr
    { $$ = AstOperator(K_FSUB, $1, $3); }
  | expr SP_FMUL expr
    { $$ = AstOperator(K_FMUL, $1, $3); }
  | expr SP_FDIV expr
    { $$ = AstOperator(K_FDIV, $1, $3); }
  | expr '+' '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign('+', $1, $4); }
  | expr '-' '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign('-', $1, $4); }
  | expr '/' '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign('/', $1, $4); }
  | expr '*' '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign('*', $1, $4); }
  | expr '&' '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign('&', $1, $4); }
  | expr '|' '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign('|', $1, $4); }
  | expr '^' '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign('^', $1, $4); }
  | expr SP_REMAINDER '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_MODULUS, $1, $4); }
  | expr SP_UNSDIV '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_UNS_DIV, $1, $4); }
  | expr SP_UNSMOD '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_UNS_MOD, $1, $4); }
  | expr SP_HIGHMULT '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_HIGHMULT, $1, $4); }
  | expr SP_UNSHIGHMULT '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_UNS_HIGHMULT, $1, $4); }
  | expr SP_SCAS '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_SCAS, $1, $4); }
  | expr SP_FRAC '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_FRAC64, $1, $4); }
  | expr SP_LIMITMIN '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_LIMITMIN, $1, $4); }
  | expr SP_LIMITMAX '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_LIMITMAX, $1, $4); }
  | expr SP_ZEROX '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_ZEROEXTEND, $1, AstOperator('+', $4, AstInteger(1))); }
  | expr SP_SIGNX '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_SIGNEXTEND, $1, AstOperator('+', $4, AstInteger(1))); }
  | expr SP_REV '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_REV, $1, $4); }
  | expr SP_REV2 '=' expr %prec SP_ASSIGN
    {
        AST *lhs = $1;
        AST *rhs = $4;
        $$ = AstOpAssign(K_REV, lhs,
                         AstOperator('+', rhs, AstInteger(1)));
    }
  | expr SP_ADDBITS '=' expr %prec SP_ASSIGN
    {
        AST *lhs = $1;
        AST *rhs = $4;
        $$ = AstOpAssign('|', lhs,
                       AstOperator(K_SHL, rhs, AstInteger(5)));
    }
  | expr SP_ADDPINS '=' expr %prec SP_ASSIGN
    {
        AST *lhs = $1;
        AST *rhs = $4;
        $$ = AstOpAssign('|', lhs,
                       AstOperator(K_SHL, rhs, AstInteger(6)));
    }
  | expr SP_ROTL '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_ROTL, $1, $4); }
  | expr SP_ROTR '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_ROTR, $1, $4); }
  | expr SP_SHL '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_SHL, $1, $4); }
  | expr SP_SHR '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_SHR, $1, $4); }
  | expr SP_SAR '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_SAR, $1, $4); }
  | expr SP_AND '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_BOOL_AND, $1, $4); }
  | expr SP_OR '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_BOOL_OR, $1, $4); }
  | expr SP_XOR '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_BOOL_XOR, $1, $4); }
  | expr '<' '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign('<', $1, $4); }
  | expr '>' '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign('>', $1, $4); }
  | expr SP_LE '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_LE, $1, $4); }
  | expr SP_GE '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_GE, $1, $4); }
  | expr SP_NE '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_NE, $1, $4); }
  | '(' expr ')' SP_RANDOM expr ':' expr %prec SP_ELSE
    { $$ = NewAST(AST_CONDRESULT, $2, NewAST(AST_THENELSE, $5, $7)); }
  | expr  SP_CONDITIONAL expr SP_CONDITIONAL_SEP expr %prec SP_CONDITIONAL
    { $$ = NewAST(AST_CONDRESULT, $1, NewAST(AST_THENELSE, $3, $5)); }
  | '(' expr ')'
    { $$ = $2; }
  | '\\' expr %prec SP_TRIPLEAT
    { $$ = AstCatch($2); }
  | funccall
    { $$ = $1; }
  | '-' expr %prec SP_NEGATE
    {
        AST *op = $2;
        /* special case -x where x is a float constant */
        if (op->kind == AST_FLOAT) {
            op->d.ival ^= 0x80000000U;
            $$ = op;
        } else {
            $$ = AstOperator(K_NEGATE, NULL, op);
        }
    }
  | SP_FSUB expr %prec SP_NEGATE
    {
        AST *op = $2;
        $$ = AstOperator(K_FNEGATE, NULL, op);
    }
  | '+' expr %prec SP_NEGATE
    {
        AST *op = $2;
        $$ = op;
    }
  | '!' expr %prec SP_BIT_NOT
    { $$ = AstOperator(K_BIT_NOT, NULL, $2); }
  | '!' '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_BIT_NOT, $3, NULL); }
  | '-' '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_NEGATE, $3, NULL); }
  | SP_NOT expr
    { $$ = AstOperator(K_BOOL_NOT, NULL, $2); }
  | SP_NOT '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_BOOL_NOT, $3, NULL); }
  | SP_FABS expr
    { $$ = AstOperator(K_FABS, NULL, $2); }
  | SP_ABS expr
    { $$ = AstOperator(K_ABS, NULL, $2); }
  | SP_ABS '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_ABS, $3, NULL); }
  | SP_FSQRT expr
    { $$ = AstOperator(K_FSQRT, NULL, $2); }
  | SP_SQRT expr
    { $$ = AstOperator(K_SQRT, NULL, $2); }
  | SP_SQRT '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_SQRT, $3, NULL); }
  | SP_DECODE expr
    { $$ = AstOperator(K_DECODE, NULL, $2); }
  | SP_DECODE '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_DECODE, $3, NULL); }
  | SP_ENCODE expr
    { $$ = AstOperator(K_ENCODE, NULL, $2); }
  | SP_ENCODE2 expr
    { $$ = AstOperator(K_ENCODE2, NULL, $2); }
  | SP_ENCODE '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_ENCODE, $3, NULL); }
  | SP_ENCODE2 '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_ENCODE2, $3, NULL); }
  | SP_QLOG expr
    { $$ = AstOperator(K_QLOG, NULL, $2); }
  | SP_QEXP expr
    { $$ = AstOperator(K_QEXP, NULL, $2); }
  | SP_NAN expr
    {   AST *op = $2;
        AST *cmp;
        cmp = AstOperator(K_FNE, op, op);
        $$ = cmp;
    }
  | SP_ONES expr
    { $$ = AstOperator(K_ONES_COUNT, NULL, $2); }
  | SP_ONES '=' expr %prec SP_ASSIGN
    { $$ = AstOpAssign(K_ONES_COUNT, $3, NULL); }
  | SP_BMASK expr
    {
        AST *ast;
        AST *expr = $2;
        ast = AstOperator(K_SHL, AstInteger(2), expr);
        ast = AstOperator('-', ast, AstInteger(1));
        $$ = ast;
    }
  | SP_BMASK '=' expr %prec SP_ASSIGN
    {
        AST *ast;
        AST *expr = $3;
        ast = AstOperator(K_SHL, AstInteger(2), expr);
        ast = AstOperator('-', ast, AstInteger(1));
        if (ExprHasSideEffects(expr)) {
            SYNTAX_ERROR("expression following BMASK= should not have side effects");
        }
        $$ = AstAssign(expr, ast);
    }
  | SP_HERE
    { $$ = NewAST(AST_HERE, NULL, NULL); }
  | lhs SP_INCREMENT
    { $$ = AstOperator(K_INCREMENT, $1, NULL); }
  | lhs SP_DECREMENT
    { $$ = AstOperator(K_DECREMENT, $1, NULL); }
  | SP_INCREMENT lhs
    { $$ = AstOperator(K_INCREMENT, NULL, $2); }
  | SP_DECREMENT lhs
    { $$ = AstOperator(K_DECREMENT, NULL, $2); }
  | lhs SP_RANDOM
    { $$ = AstOperator('?', $1, NULL); }
  | SP_RANDOM lhs
    { $$ = AstOperator('?', NULL, $2); }
  | lhs '~'
    { $$ = NewAST(AST_POSTSET, $1, AstInteger(0)); }
  | lhs SP_DOUBLETILDE
    { $$ = NewAST(AST_POSTSET, $1, AstInteger(-1)); }
  | '~' lhs
    { AST *shf;
      AST *val = $2;
      shf = AstOpAssign(K_SIGNEXTEND, val, AstInteger(8));
      $$ = shf;
    }
  | SP_DOUBLETILDE lhs
    { AST *shf;
      AST *val = $2;
      shf = AstOpAssign(K_SIGNEXTEND, val, AstInteger(16));
      $$ = shf;
    }
  | SP_CONSTANT '(' expr ')'
    { $$ = NewAST(AST_CONSTANT, $3, NULL); }
  | SP_ALLOCA '(' expr ')'
    { $$ = NewAST(AST_ALLOCA, NULL, $3); }
  | SP_FLOAT '(' expr ')'
    { $$ = NewAST(AST_TOFLOAT, $3, NULL); }
  | SP_ROUND '(' expr ')'
    { $$ = NewAST(AST_ROUND, $3, NULL); }
  | SP_TRUNC '(' expr ')'
    { $$ = NewAST(AST_TRUNC, $3, NULL); }
  | lookupexpr
    { $$ = $1; }
  | lookdownexpr
    { $$ = $1; }
  | SP_IF expr SP_THEN expr SP_ELSE expr
    { $$ = NewAST(AST_CONDRESULT, $2, NewAST(AST_THENELSE, $4, $6)); }
  ;

lhs: identifier
  | identifier '[' expr ']'
    {
        $$ = NewAST(AST_ARRAYREF, $1, $3);
    }
  | identifier '.' identifier '[' expr ']'
    {
        AST *objroot = $1;
        AST *method = $3;
        AST *index = $5;
        AST *expr;
        expr = NewAST(AST_METHODREF, objroot, method);
        expr = NewAST(AST_ARRAYREF, expr, index);
        $$ = expr;
    }
  | identifier '[' expr ']' '.' '[' range ']'
    {
        AST *arr = NewAST(AST_ARRAYREF, $1, $3);
        AST *range = $7;
        $$ = NewAST(AST_RANGEREF, arr, range);
    }
  | hwreg
    { $$ = $1; }
  | hwreg '[' range ']'
    { $$ = NewAST(AST_RANGEREF, $1, $3);
    }
  | hwreg '[' '#' '#' range ']'
    {
        AST *reg = $1;
        AST *index = $5;
        AST *base = NewAST(AST_RANGEREF, reg, index);
        AST *holder = NewAST(AST_BIGIMMHOLDER, base, NULL);
        $$ = holder;
    }
  | hwreg '.' '[' range ']'
    { $$ = NewAST(AST_RANGEREF, $1, $4);
    }
  | memref '[' expr ']'
    { $$ = NewAST(AST_ARRAYREF, $1, $3); }
  | memref '[' expr ']' '.' '[' range ']'
    {
        AST *ast = NewAST(AST_ARRAYREF, $1, $3);
        AST *range = $7;
        $$ = NewAST(AST_RANGEREF, ast, range);
    }
  | memref '.' '[' range ']'
    { $$ = NewAST(AST_RANGEREF, $1, $4); }
  | '(' expr ')' '[' expr ']'
    { $$ = NewAST(AST_ARRAYREF, $2, $5); }
  | memref
    { $$ = NewAST(AST_ARRAYREF, $1, AstInteger(0)); }
  | SP_SPR '[' expr ']'
    { $$ = AstSprRef($3, 0x1f0); }
  | SP_COGREG '[' expr ']'
    { $$ = AstSprRef($3, 0x0); }
  | SP_COGREG '[' expr ']' '[' expr ']'
    {
        AST *cogmem = $3;
        AST *off = $6;
        $$ = AstSprRef(AstOperator('+', cogmem, off), 0x0);
    }
  | identifier '.' '[' range ']'
    { $$ = NewAST(AST_RANGEREF, $1, $4);
    }
  | SP_FIELD '[' expr ']'
    {
        AST *ref = $3;
        AST *idx = AstInteger(0);
        $$ = NewAST(AST_FIELDREF, ref, idx);
    }
  | SP_FIELD '[' expr ']' '[' expr ']'
    {
        AST *ref = $3;
        AST *idx = $6;
        $$ = NewAST(AST_FIELDREF, ref, idx);
    }
  ;

lhsseq:
  '(' lhssingle ',' lhsseqcont ')'
    { $$ = NewAST(AST_EXPRLIST, $2, $4); }
  | lhssingle ',' lhsseqcont
    { $$ = NewAST(AST_EXPRLIST, $1, $3); }
  ;

lhsseqcont:
  lhssingle
    { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
  | lhsseqcont ',' lhssingle
    { $$ = AddToList($1, NewAST(AST_EXPRLIST, $3, NULL)); }
;

lhssingle:
    lhs
       { $$ = $1; }
    | SP_EMPTY
       { $$ = NewAST(AST_EMPTY, NULL, NULL); }
;

memref:
  SP_BYTE '[' expr ']'
    { $$ = NewAST(AST_MEMREF, ast_type_byte, $3); }
  | SP_WORD '[' expr ']'
    { $$ = NewAST(AST_MEMREF, ast_type_word, $3); }
  | SP_LONG '[' expr ']'
    { $$ = NewAST(AST_MEMREF, ast_type_long, $3); }
  | identifier '.' SP_BYTE
    { $$ = NewAST(AST_MEMREF, ast_type_byte, NewAST(AST_ADDROF, $1, NULL)); }
  | identifier '.' SP_WORD
    { $$ = NewAST(AST_MEMREF, ast_type_word, NewAST(AST_ADDROF, $1, NULL)); }
  | identifier '.' SP_LONG
    { $$ = NewAST(AST_MEMREF, ast_type_long, NewAST(AST_ADDROF, $1, NULL)); }
;

opt_numrets:
  /* nothing */
    { $$ = NULL; }
  | ':' SP_NUM
    { $$ = $2; }
;

funccall:
  lhs '(' exprlist ')' opt_numrets
    { $$ = MakeFunccall($1, FixupList($3), $5); }
  | lhs '(' ')' opt_numrets
    {
        $$ = MakeFunccall($1, NULL, $4);
        LANGUAGE_WARNING(LANG_SPIN_SPIN1, NULL, "Using () for functions with no parameters is a flexspin extension");
    }
  | SP_PINW '(' rangeexpritem ',' expritem ')'
    {
        AST *arg1 = ConvertPinRange($3);
        AST *arg2 = $5;
        AST *ident = AstIdentifier("pinw");
        arg1 = AddToList(arg1, arg2);
        
        $$ = MakeFunccall(ident, arg1, NULL);
    }
  | SP_PINR '(' rangeexpritem ')'
    {
        AST *arg1 = ConvertPinRange($3);
        AST *ident = AstIdentifier("pinr");
        
        $$ = MakeFunccall(ident, arg1, NULL);
    }
  | SP_PINL '(' rangeexpritem ')'
    {
        AST *arg1 = ConvertPinRange($3);
        AST *ident = AstIdentifier("pinl");
        
        $$ = MakeFunccall(ident, arg1, NULL);
    }
  | SP_PINH '(' rangeexpritem ')'
    {
        AST *arg1 = ConvertPinRange($3);
        AST *ident = AstIdentifier("pinh");
        
        $$ = MakeFunccall(ident, arg1, NULL);
    }
  | SP_PINT '(' rangeexpritem ')'
    {
        AST *arg1 = ConvertPinRange($3);
        AST *ident = AstIdentifier("pintoggle");
        
        $$ = MakeFunccall(ident, arg1, NULL);
    }
  | SP_COGINIT '(' exprlist ')'
    {
        $$ = NewAST(AST_COGINIT, FixupList($3), NULL);
    }
  | SP_COGNEW '(' exprlist ')'
    {
        AST *elist;
        AST *immval = AstInteger(0x1e); // works to cognew both P1 and P2
        elist = NewAST(AST_EXPRLIST, immval, NULL);
        elist = AddToList(elist, $3);
        elist = FixupList(elist);
        $$ = NewAST(AST_COGINIT, elist, NULL);
        LANGUAGE_WARNING(LANG_SPIN_SPIN2, NULL, "cognew support in Spin2 is a flexspin extension");
    }
  | identifier '.' identifier '(' exprlist ')' opt_numrets
    { 
        $$ = MakeFunccall(NewAST(AST_METHODREF, $1, $3), FixupList($5), $7);
    }
  | identifier '.' identifier '(' ')' opt_numrets
    { 
        $$ = MakeFunccall(NewAST(AST_METHODREF, $1, $3), NULL, $6);
        LANGUAGE_WARNING(LANG_SPIN_SPIN1, NULL, "Using () for functions with no parameters is a flexspin extension");
    }
  | identifier '.' identifier
    { 
        $$ = NewAST(AST_METHODREF, $1, $3);
    }
  | identifier '[' expr ']' '.' identifier '(' exprlist ')' opt_numrets
    { 
        AST *arr = NewAST(AST_ARRAYREF, $1, $3);
        $$ = MakeFunccall(NewAST(AST_METHODREF, arr, $6), FixupList($8), $10);
    }
  | identifier '[' expr ']' '.' identifier '(' ')' opt_numrets
    { 
        AST *arr = NewAST(AST_ARRAYREF, $1, $3);
        $$ = MakeFunccall(NewAST(AST_METHODREF, arr, $6), NULL, $9);
        LANGUAGE_WARNING(LANG_SPIN_SPIN1, NULL, "Using () for functions with no parameters is a flexspin extension");
    }
  | identifier '[' expr ']' '.' identifier
    { 
        AST *arr = NewAST(AST_ARRAYREF, $1, $3);
        $$ = NewAST(AST_METHODREF, arr, $6);
    }
  | SP_REGEXEC '(' exprlist ')'
    {
        SYNTAX_ERROR("REGEXEC is not supported by flexspin");
    }
  | SP_REGLOAD '(' exprlist ')'
    {
        SYNTAX_ERROR("REGLOAD is not supported by flexspin");
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

datexpritem:
  expr
       { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
   | expr SP_DAT_LBRACK expr SP_DAT_RBRACK
       { $$ = NewAST(AST_EXPRLIST, NewAST(AST_ARRAYDECL, $1, $3), NULL); }
   | SP_LONG datexpritem
       { $$ = NewAST(AST_EXPRLIST, NewAST(AST_LONGLIST, $2, NULL), NULL); }
   | SP_WORD datexpritem
       { $$ = NewAST(AST_EXPRLIST, NewAST(AST_WORDLIST, $2, NULL), NULL); }
   | SP_BYTE datexpritem
       { $$ = NewAST(AST_EXPRLIST, NewAST(AST_BYTELIST, $2, NULL), NULL); }
   | SP_WORDFIT datexpritem
       {
           AST *dat = NewAST(AST_EXPRLIST, NewAST(AST_WORDFITLIST, $2, NULL), NULL);
           dat->d.ival = 1; // make it fit
           $$ = dat;
       }
   | SP_BYTEFIT datexpritem
       {
           AST *dat = NewAST(AST_EXPRLIST, NewAST(AST_BYTEFITLIST, $2, NULL), NULL);
           $$ = dat;
       }
   | SP_FVAR datexpritem
       { $$ = NewAST(AST_EXPRLIST, NewAST(AST_FVAR_LIST, $2, NULL), NULL); }
   | SP_FVARS datexpritem
       { $$ = NewAST(AST_EXPRLIST, NewAST(AST_FVARS_LIST, $2, NULL), NULL); }
;

datexprlist:
  datexpritem
 | datexprlist ',' datexpritem
   { $$ = AddToList($1, $3); }
 ;

optcatch:
/* nothing */
    { $$ = NULL; }
 | '\\'
    { $$ = NewAST(AST_CATCH, NULL, NULL); }
 ;

operand:
  expr
   { $$ = NewAST(AST_EXPRLIST, $1, NULL); }
 | '#' optcatch expr
   {
       AST *catchexpr = $2;
       AST *expr = $3;
       if (catchexpr) {
           catchexpr->left = expr;
           expr = catchexpr;
       }
       $$ = NewAST(AST_EXPRLIST, NewAST(AST_IMMHOLDER, expr, NULL), NULL);
   }
 | '#' '#' optcatch expr
   {
       AST *catchexpr = $3;
       AST *expr = $4;
       if (catchexpr) {
           catchexpr->left = expr;
           expr = catchexpr;
       }
       $$ = NewAST(AST_EXPRLIST, NewAST(AST_BIGIMMHOLDER, expr, NULL), NULL);
   }
 | expr '[' expr ']'
   {
       /* this is an extra rule for ptra++[Y] */
       $$ = NewAST(AST_EXPRLIST, NewAST(AST_ARRAYREF, $1, $3), NULL);
   }
;

operandlist:
   operand
   { $$ = $1; }
 | operandlist ',' operand
   { $$ = AddToList($1, $3); }
 ;

range:
  expr
    { $$ = NewAST(AST_RANGE, $1, NULL); }
  | expr SP_DOTS expr
    { $$ = NewAST(AST_RANGE, $1, $3); }
  ;

integer:
  SP_NUM
;

float:
  SP_FLOATNUM
;

string:
  SP_STRING
;

identifier:
  SP_IDENTIFIER
  { $$ = $1; }
  | SP_RESULT
  { $$ = NewAST(AST_RESULT, NULL, NULL); }
;

annotation:
  SP_ANNOTATION
  { $$ = $1; }
;


hwreg:
  SP_HWREG
  { $$ = $1; }
;

instruction:
  SP_INSTR
    { $$ = $1; }
  | '<' SP_INSTR
    {
        AST *ast = $2;
        AST *rootast = $2;
        AST *parent = NULL;
        while (ast->kind == AST_SRCCOMMENT || ast->kind == AST_COMMENT)
        {
            parent = ast;
            ast = ast->right;
        }
        ast = NewAST(AST_COMPRESS_INSTR, ast, NULL);
        if (parent) {
            parent->right = ast;
            $$ = rootast;
        } else {
            $$ = ast;
        }
    }
  | instrmodifier instruction
    { $$ = AddToList($2, $1); }
;
 
instrmodifier:
  SP_INSTRMODIFIER
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
spinyyerror(const char *msg)
{
    extern int saved_spinyychar;
    int yychar = saved_spinyychar;
    
    SETCOLOR(PRINT_ERROR);
    ERRORHEADER(current->Lptr->fileName, current->Lptr->lineCounter, "error");

    if (!strcmp(msg, "syntax error, unexpected $end")) {
        fprintf(stderr, "syntax error, unexpected end of file");
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
