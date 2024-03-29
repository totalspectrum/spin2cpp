//
// Microtests for lexical analyzer
//
#define TESTLEX

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "spinc.h"
#include "frontends/lexer.h"
#include "spin.tab.h"

#ifndef LANG_DEFAULT
#define LANG_DEFAULT 0
#endif

//typedef enum yytokentype Token;
typedef int Token;

Module *current;
Module *allparse;
Module *systemModule;
Function *curfunc;
SymbolTable *currentTypes;

AST *ast_type_long, *ast_type_word, *ast_type_byte, *ast_type_float;
AST *ast_type_string, *ast_type_generic;
AST *ast_type_generic_funcptr;
AST *ast_type_ptr_byte;
AST *ast_type_unsigned_long;
AST *ast_type_void;
AST *ast_type_long64, *ast_type_unsigned_long64, *ast_type_float64;
AST *ast_type_c_boolean, *ast_type_basic_boolean;
AST *ast_type_c_boolean_small, *ast_type_basic_boolean_small;

int gl_p2 = 0;
int gl_errors = 0;
int gl_have_lut = 0;
int gl_output = 0;
int gl_interp_kind = 0;
int gl_gas_dat = 0;
int gl_normalizeIdents = 1;  /* for compatibility with ident tests */
int gl_src_charset = CHARSET_UTF8;
int gl_run_charset = CHARSET_UTF8;
int gl_dat_offset = 0;
int gl_infer_ctypes = 0;
int gl_fixedreal = 0;
int gl_debug = 1;
int gl_brkdebug = 1;
const char *gl_intstring = "int32_t";
int gl_warn_flags = 0;
int gl_nostdlib = 0;
int gl_in_spin2_funcbody = 0;

// dummy needed for some symbol lookups
Module *GetTopLevelModule(void) {
    return 0;
}

// another dummy
int BCgetDAToffset(Module *P, bool absolute, AST *errloc, bool printErrors) {
    return -1;
}

static void EXPECTEQfn(long x, long val, int line) {
    if (x != val) {
        fprintf(stderr, "test failed at line %d of %s: expected %ld got %ld\n",
                line, __FILE__, val, x);
        abort();
    }
}

//
// dummy definition
//
AST *AstTempLocalVariable(const char *prefix, AST *type)
{
    return NULL;
}

#define EXPECTEQ(x, y) EXPECTEQfn((x), (y), __LINE__)

static void
testNumber(const char *str, uint32_t val)
{
    AST *ast;
    LexStream L;
    Token t;
    int c;
    printf("testing number[%s]...", str);
    strToLex(&L, str, strlen(str), NULL, LANG_DEFAULT);
    t = getSpinToken(&L, &ast);
    EXPECTEQ(t, SP_NUM);
    c = lexgetc(&L);
    EXPECTEQ(c, EOF);
    assert(ast != NULL);
    assert(ast->kind == AST_INTEGER);
    EXPECTEQ(ast->d.ival, val);
    printf("passed\n");
}

static void
testFloat(const char *str, float fval)
{
    AST *ast;
    LexStream L;
    Token t;
    int c;
    uint32_t val;
    union {
        uint32_t i;
        float f;
    } v;
    v.f = fval;
    val = v.i;

    printf("testing number[%s]...", str);
    strToLex(&L, str, strlen(str), NULL, LANG_DEFAULT);
    t = getSpinToken(&L, &ast);
    EXPECTEQ(t, SP_FLOATNUM);
    c = lexgetc(&L);
    EXPECTEQ(c, EOF);
    assert(ast != NULL);
    assert(ast->kind == AST_FLOAT);
    EXPECTEQ(ast->d.ival, val);
    printf("passed\n");
}

static void
testIdentifier(const char *str, const char *expect)
{
    LexStream L;
    AST *ast;
    Token t;

    strToLex(&L, str, strlen(str), NULL, LANG_DEFAULT);
    t = getSpinToken(&L, &ast);
    EXPECTEQ(t, SP_IDENTIFIER);
    assert(ast != NULL);
    assert(ast->kind == AST_IDENTIFIER);
    EXPECTEQ(0, strcmp(ast->d.string, expect));
    printf("from [%s] read identifier [%s]\n", str, ast->d.string);
}

static void
testTokenStream(const char *str, int *tokens, int numtokens)
{
    int i;
    LexStream L;
    AST *ast;
    Token t;

    printf("testing tokens [%s]...", str); fflush(stdout);
    strToLex(&L, str, strlen(str), NULL, LANG_DEFAULT);
    for (i = 0; i < numtokens; i++) {
        t = getSpinToken(&L, &ast);
        EXPECTEQ(t, tokens[i]);
    }
    printf("passed\n");
}

static void
testTokenStream2(const char *str, int *tokens, int numtokens)
{
    int i;
    LexStream L;
    AST *ast;
    Token t;

    printf("testing tokens [%s]...", str); fflush(stdout);
    strToLex(&L, str, strlen(str), NULL, LANG_SPIN_SPIN2);
    for (i = 0; i < numtokens; i++) {
        t = getSpinToken(&L, &ast);
        EXPECTEQ(t, tokens[i]);
    }
    printf("passed\n");
}

void
ERROR(AST *instr, const char *msg, ...)
{
    va_list args;
    LineInfo *info = GetLineInfo(instr);
    
    if (info) {
        fprintf(stderr, "Error on line %d: ", info->lineno);
    } else {
        fprintf(stderr, "Error: ");
    }
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void
SYNTAX_ERROR(const char *msg, ...)
{
    va_list args;
    
    fprintf(stderr, "Error: ");
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void
WARNING(AST *instr, const char *msg, ...)
{
    va_list args;
    LineInfo *info = GetLineInfo(instr);
    
    if (info)
        fprintf(stderr, "Warning on line %d: ", info->lineno);
    else
        fprintf(stderr, "Warning: ");

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void
ERROR_UNKNOWN_SYMBOL(AST *ast)
{
    ERROR(ast, "Unknown symbol %s", ast->d.string);
}

#define N_ELEM(x) (sizeof(x)/sizeof(x[0]))
static int tokens0[] = { SP_NUM, '+', SP_NUM, SP_EOLN, SP_EOF };
static int tokens1[] = { SP_IDENTIFIER, '-', SP_NUM, '+', SP_IDENTIFIER, SP_EOLN, SP_EOF };
static int tokens2[] = { SP_CON, SP_CON, SP_IDENTIFIER, SP_CON, SP_NUM, SP_EOLN, SP_EOF };
static int tokens3[] = { SP_LIMITMAX, SP_LIMITMIN, '<', SP_LE, SP_GE, '>', SP_EQ, '=', '&', '<' };

static const char *token4test = "pub \r\n  if\n    foo\n  bar\n";
static int tokens4[] = { SP_PUB, SP_EOLN, SP_IF, SP_EOLN, SP_INDENT, SP_IDENTIFIER, SP_EOLN, SP_OUTDENT, SP_IDENTIFIER, SP_EOLN };

static const char *token5test = "pub\n  if\n    foo\n      repeat\n  else";
static int tokens5[] = { SP_PUB, SP_EOLN, SP_IF, SP_EOLN, SP_INDENT, SP_IDENTIFIER, SP_EOLN, SP_REPEAT, SP_EOLN, SP_INDENT, SP_OUTDENT, SP_OUTDENT, SP_ELSE};

static const char *token6test = "dat\nlabel if_z add x,#1 wc";
static int tokens6[] = { SP_DAT, SP_EOLN, SP_IDENTIFIER, SP_INSTRMODIFIER, SP_INSTR,
                         SP_IDENTIFIER, ',', '#', SP_NUM, SP_INSTRMODIFIER};

static const char *token7test = 
"pub f\n"
"   foo\n"
"  repeat while x\n"
"  return x\n";

static int tokens7[] = 
{ 
  SP_PUB, SP_IDENTIFIER, SP_EOLN,
  SP_IDENTIFIER, SP_EOLN,
  SP_REPEAT, SP_WHILE, SP_IDENTIFIER, SP_EOLN,
  SP_INDENT, SP_OUTDENT,
  SP_RETURN, SP_IDENTIFIER, SP_EOLN, SP_EOF
};

static const char *token8test = 
"pub f\n"
"   foo\n"
"  repeat while x.y[8] < z\n"
"\n"
"pri f\n"
"  return x\n";

static int tokens8[] = 
{ 
  SP_PUB, SP_IDENTIFIER, SP_EOLN,
  SP_IDENTIFIER, SP_EOLN,
  SP_REPEAT, SP_WHILE, SP_IDENTIFIER, '.', SP_IDENTIFIER, '[', SP_NUM, ']', '<', SP_IDENTIFIER, SP_EOLN,
  SP_INDENT, SP_OUTDENT,
  SP_PRI, SP_IDENTIFIER, SP_EOLN,
  SP_RETURN, SP_IDENTIFIER, SP_EOLN, SP_EOF
};

static const char *token9test = 
"pub f\n"
"  debug(`hello `(a))\n"
;

static int tokens9[] = 
{ 
  SP_PUB, SP_IDENTIFIER, SP_EOLN,
  SP_DEBUG, '(', SP_BACKTICK_STRING, ',', SP_IDENTIFIER, '(', SP_IDENTIFIER, ')', ')', SP_EOLN,
  SP_EOF
};

static const char *token10test = 
"pub f\n"
"  debug(`hello `(a) done)\n"
;

static int tokens10[] = 
{ 
  SP_PUB, SP_IDENTIFIER, SP_EOLN,
  SP_DEBUG, '(', SP_BACKTICK_STRING, ',', SP_IDENTIFIER, '(', SP_IDENTIFIER, ')', ',', SP_BACKTICK_STRING, ')', SP_EOLN,
  SP_EOF
};

static const char *token11test = 
"pub f\n"
"  repeat\n"
"    debug(`MyTerm 1 'j=`(j): Temp = `(i)')\n"
;

static int tokens11[] = 
{ 
  SP_PUB, SP_IDENTIFIER, SP_EOLN,
  SP_REPEAT, SP_EOLN,
  SP_INDENT,
  SP_DEBUG, '(', SP_BACKTICK_STRING, ',',
  SP_IDENTIFIER, '(', SP_IDENTIFIER, ')', ',',
  SP_BACKTICK_STRING, ',',
  SP_IDENTIFIER, '(', SP_IDENTIFIER, ')', ',' ,
  SP_BACKTICK_STRING, ')', SP_EOLN,
  SP_EOF
};

int
main()
{
    initSpinLexer(0);

    testTokenStream("1 + 1", tokens0, N_ELEM(tokens0));
    testTokenStream("{a comment line} $1 + 1", tokens0, N_ELEM(tokens0));
    testTokenStream("{{a nested comment}} $1 + 1", tokens0, N_ELEM(tokens0));
    testTokenStream("{{{a nested doc comment}} $1 + 1", tokens0, N_ELEM(tokens0));
    testTokenStream("x-1+y", tokens1, N_ELEM(tokens1));
    testTokenStream("_x0{some comment 1} - 1 + y_99", tokens1, N_ELEM(tokens1));
    testTokenStream("con CON con99 Con 99", tokens2, N_ELEM(tokens2));
    testTokenStream("<# #> < =< => > == = &<", tokens3, N_ELEM(tokens3));
    testTokenStream(token4test, tokens4, N_ELEM(tokens4));
    testTokenStream(token5test, tokens5, N_ELEM(tokens5));
    testTokenStream(token6test, tokens6, N_ELEM(tokens6));
    testTokenStream(token7test, tokens7, N_ELEM(tokens7));
    testTokenStream(token8test, tokens8, N_ELEM(tokens8));

    testTokenStream2(token9test, tokens9, N_ELEM(tokens9));
    testTokenStream2(token10test, tokens10, N_ELEM(tokens9));
    testTokenStream2(token11test, tokens11, N_ELEM(tokens9));
    
    testNumber("0", 0);
    testNumber("00", 0);
    testNumber("007", 7);
    testNumber("008", 8);
    testNumber("\t \t 123", 123);
    testNumber("65535", 65535);
    testNumber("  $41", 65);
    testNumber("$01_ff", 511);
    testNumber("$A5", 165);
    testNumber("%101", 5);
    testNumber("%11", 3);
    testNumber("%%31", 13);
    testNumber("80_000_000", 80000000);

    testFloat("1.0", 1.0f);
    testFloat("2.0", 2.0f);
    testFloat("0.01", 0.01f);
    testFloat(".5", 0.5f);
    testFloat("1.0e-2", 0.01f);
    testFloat("1.e-2", 0.01f);
    testFloat("3.14e5", 314000.0f);

    testIdentifier("x99+8", "X99");
    testIdentifier("_a_b", "_A_b");
    printf("all tests passed\n");
    return 0;
}

/* dummy functions to make the linker happy */
void defaultVariable() { }
void defaultBuiltin() { }
void str1Builtin() { }
void strcompBuiltin() { }
void memBuiltin() { }
void memFillBuiltin() { }
void waitpeqBuiltin() { }
void rebootBuiltin() { }

// and another dummy for testing
void PrintExpr(Flexbuf *f, AST *expr, int flags) {
    flexbuf_addstr(f, "expression");
}

// dummy for tracking source files
void AddSourceFile(const char *shortName, const char *longName) {
}

int GetCurrentLang() { return LANG_DEFAULT; }
