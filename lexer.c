//
// Simple lexical analyzer for a language where indentation
// is significant
//
// Copyright (c) 2011 Total Spectrum Software Inc.
//
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include "spinc.h"

SymbolTable reservedWords;

/* functions for handling string streams */
static int 
strgetc(LexStream *L)
{
    char *s;
    int c;

    s = (char *)L->ptr;
    c = (*s++) & 0x00ff;
    L->ptr = s;
    return (c == 0) ? T_EOF : c;
}

static void
strungetc(LexStream *L, int c)
{
    char *s = L->ptr;

    if (s != L->arg) {
        --s;
        /* the source string may be constant, so only
           try to write if we have to */
        if (*s != c && c != T_EOF) {
            *s = c;
        }
        L->ptr = s;
    }
}

/* open a stream from a string s */
void strToLex(LexStream *L, const char *s)
{
    memset(L, 0, sizeof(*L));
    L->arg = L->ptr = (void *)s;
    L->getcf = strgetc;
    L->ungetcf = strungetc;
    L->lineCounter = 1;
    L->fileName = "<string>";
}

/* functions for handling FILE streams */
static int 
filegetc(LexStream *L)
{
    FILE *f;
    int c;

    f = (FILE *)L->ptr;
    c = fgetc(f);
    return (c >= 0) ? c : T_EOF;
}

static void
fileungetc(LexStream *L, int c)
{
    FILE *f = L->ptr;

    if (c != T_EOF) {
        ungetc(c, f);
    }
}

/* open a stream from a FILE f */
void fileToLex(LexStream *L, FILE *f, const char *name)
{
    memset(L, 0, sizeof(*L));
    L->ptr = (void *)f;
    L->arg = NULL;
    L->getcf = filegetc;
    L->ungetcf = fileungetc;
    L->lineCounter = 1;
    L->fileName = name;
}


/*
 * utility functions
 */
int
isIdentifierStart(int c)
{
    if (isalpha(c))
        return 1;
    if (c == '_')
        return 1;
    return 0;
}

int
isIdentifierChar(int c)
{
    return isIdentifierStart(c) || isdigit(c);
}

/*
 * actual parsing functions
 */
static int
parseNumber(LexStream *L, unsigned int base, uint32_t *num)
{
    unsigned long uval, digit;
    unsigned int c;

    uval = 0;

    for(;;) {
        c = lexgetc(L);
        if (c == '_')
            continue;
        else if (c >= 'A' && c <= 'Z')
            digit = 10 + c - 'A';
        else if (c >= 'a' && c <= 'z')
            digit = 10 + c - 'a';
        else
            digit = (c - '0');
        if (digit < base)
            uval = base * uval + digit;
        else
            break;
    }
    lexungetc(L, c);
    *num = uval;
    return T_NUM;
}

/* dynamically grow a string */
#define INCSTR 16

static void
addchar(int c, char **place, size_t *space, size_t *len)
{
    if (*len + 1 > *space) {
        *space += 16;
        *place = realloc(*place, *space);
    }
    assert(*place != NULL);
    (*place)[*len] = c;
    *len += 1;
}

/* parse an identifier */
static int
parseIdentifier(LexStream *L, AST **ast_ptr)
{
    int c;
    char *place = NULL;
    size_t space = 0;
    size_t len = 0;
    Symbol *sym;
    AST *ast;

    c = lexgetc(L);
    while (isIdentifierChar(c)) {
        addchar(c, &place, &space, &len);
        c = lexgetc(L);
    }
    addchar('\0', &place, &space, &len);
    lexungetc(L, c);

    /* check for reserved words */
    sym = FindSymbol(&reservedWords, place);
    if (sym != NULL) {
        free(place);
        if (sym->type == SYM_RESERVED) {
            c = INTVAL(sym);
            /* check for special handling */
            if (c == T_PUB || c == T_PRI) {
                L->in_func = 1;
            } else if (c == T_DAT || c == T_OBJ || c == T_VAR || c == T_CON) {
                L->in_func = 0;
            }
            return c;
        }
        if (sym->type == SYM_INSTR) {
            ast = NewAST(AST_INSTR, NULL, NULL);
            ast->d.ptr = sym->val;
            *ast_ptr = ast;
            return T_INSTR;
        }
        if (sym->type == SYM_HWREG) {
            ast = NewAST(AST_HWREG, NULL, NULL);
            ast->d.ptr = sym->val;
            *ast_ptr = ast;
            return T_HWREG;
        }
        fprintf(stderr, "Internal error: Unknown symbol type %d\n", sym->type);
    }

    ast = NewAST(AST_IDENTIFIER, NULL, NULL);
    ast->d.string = place;
    *ast_ptr = ast;
    return T_IDENTIFIER;
}

//
// skip over comments and spaces
// return first non-comment non-space character
// if we are inside a function, emit T_INDENT when
// we increase the indent level, T_OUTDENT when we
// decrease it
//
#define TAB_STOP 8

int
skipSpace(LexStream *L)
{
    int c;
    int commentNest;
    int indent = 0;
again:
    c = lexgetc(L);
    while (c == ' ' || c == '\t') {
        if (L->eoln) {
            if (c == '\t') {
                indent = TAB_STOP * ((indent + (TAB_STOP-1))/TAB_STOP);
            } else {
                indent++;
            }
        }
        c = lexgetc(L);
    }
    if (L->in_func && L->eoln) {
        if (indent > L->indent) {
            lexungetc(L, c);
            L->indent = indent;
            L->eoln = 0;
            return T_INDENT;
        }
        if (indent < L->indent) {
            lexungetc(L, c);
            L->indent = indent;
            L->eoln = 0;
            return T_OUTDENT;
        }
    }
    L->eoln = 0;

    /* single quote comments */
    if (c == '\'') {
        do {
            c = lexgetc(L);
        } while (c != '\n' && c != T_EOF);
        if (c == '\n') {
            L->lineCounter++;
            L->eoln = 1;
            goto again;
        }
    }
    if (c == '{') {
        commentNest = 1;
        do {
            c = lexgetc(L);
            if (c == '\n')
                L->lineCounter++;
            if (c == '{')
                commentNest++;
            else if (c == '}')
                --commentNest;
        } while (commentNest > 0 && c != T_EOF);
        if (c == T_EOF)
            return c;
        goto again;
    }
    if (c == '\n') {
        L->lineCounter++;
        L->eoln = 1;
        return T_EOLN;
    }
    return c;
}

static char operator_chars[] = "-+*/|<>=!@~#^:.";

int
getToken(LexStream *L, AST **ast_ptr)
{
//    int base = 10;
    int c;
    AST *ast = NULL;

    c = skipSpace(L);

    if (isdigit(c)) {
        lexungetc(L,c);
        ast = NewAST(AST_INTEGER, NULL, NULL);
        c = parseNumber(L, 10, &ast->d.ival);
    } else if (c == '$') {
        ast = NewAST(AST_INTEGER, NULL, NULL);
        c = parseNumber(L, 16, &ast->d.ival);
    } else if (c == '%') {
        ast = NewAST(AST_INTEGER, NULL, NULL);
        c = lexgetc(L);
        if (c == '%') {
            c = parseNumber(L, 4, &ast->d.ival);
        } else {
            lexungetc(L, c);
            c = parseNumber(L, 2, &ast->d.ival);
        }
    } else if (isIdentifierStart(c)) {
        lexungetc(L, c);
        c = parseIdentifier(L, &ast);
    } else if (strchr(operator_chars, c) != NULL) {
        char op[6];
        int i;
        int token;
        Symbol *sym = NULL;

        op[0] = token = c;
        for (i = 1; i < sizeof(op)-1; i++) {
            c = lexgetc(L);
            if (strchr(operator_chars, c) == NULL) {
                lexungetc(L, c);
                break;
            }
            op[i] = c;
            op[i+1] = 0;
            sym = FindSymbol(&reservedWords, op);
            if (sym) {
                token = INTVAL(sym);
            } else {
                lexungetc(L, c);
                break;
            }
        }
        c = token;
    }
    *ast_ptr = ast;
    return c;
}


/*
 * function to initialize the lexer
 */
struct reservedword {
    const char *name;
    intptr_t val;
} init_words[] = {
    { "byte", T_BYTE },
    { "con", T_CON },
    { "cognew", T_COGNEW },
    { "dat", T_DAT },

    { "else", T_ELSE },
    { "elseif", T_ELSEIF },
    { "elseifnot", T_ELSEIFNOT },
    { "if", T_IF },
    { "ifnot", T_IFNOT },

    { "long", T_LONG },

    { "obj", T_OBJ },
    { "pri", T_PRI },
    { "pub", T_PUB },
    { "repeat", T_REPEAT },
    { "return", T_RETURN },

    { "var", T_VAR },

    { "word", T_WORD },

    /* operators */
    { "+", '+' },
    { "-", '-' },
    { "/", '/' },
    { "//", T_MODULUS },
    { "*", '*' },
    { "**", T_HIGHMULT },
    { ">", '>' },
    { "<", '<' },
    { "=<", T_LE },
    { "=>", T_GE },
    { "=", '=' },
    { "==", T_EQ },
    { "<>", T_NE },

    { "<<", T_SHL },
    { ">>", T_SHR },
    { "~>", T_SAR },

    { ":=", T_ASSIGN },
    { "..", T_DOTS },
    { "|<", T_DECODE },
    { ">|", T_ENCODE },
    { "||", T_ABS },
    { "#>", T_LIMITMIN },
    { "#<", T_LIMITMAX },
};

void
initLexer(void)
{
    int i;

    /* add our reserved words */
    for (i = 0; i < N_ELEMENTS(init_words); i++) {
        AddSymbol(&reservedWords, init_words[i].name, SYM_RESERVED, (void *)init_words[i].val);
    }

    /* add the PASM instructions */
    InitPasm();
}

static void
nofunc(FILE *f, AST *expr)
{
    ERROR("bad assembly instruction in spin code");
}

Instruction
instr[] = {
    { "abs",    0xa8800000, TWO_OPERANDS, nofunc },
    { "absneg", 0xac800000, TWO_OPERANDS, nofunc },
    { "add",    0x80800000, TWO_OPERANDS, nofunc },
    { "addabs", 0x88800000, TWO_OPERANDS, nofunc },
    { "adds",   0xd0800000, TWO_OPERANDS, nofunc },
    { "addsx",  0xd8800000, TWO_OPERANDS, nofunc },
    { "addx",   0xc8800000, TWO_OPERANDS, nofunc },
    { "and",    0x60800000, TWO_OPERANDS, nofunc },
    { "andn",   0x64800000, TWO_OPERANDS, nofunc },

    { "call",   0x5cc00000, CALL_OPERAND, nofunc },
    { "clkset", 0x0c400000, ONE_OPERAND, nofunc },
    { "cmp",    0x84000000, TWO_OPERANDS, nofunc },
    { "cmps",   0xc0000000, TWO_OPERANDS, nofunc },
    { "cmpsx",  0xc4000000, TWO_OPERANDS, nofunc },
    { "cmpx",   0xcc000000, TWO_OPERANDS, nofunc },

    { "cogid",  0x0c400001, ONE_OPERAND, nofunc },
    { "coginit", 0x0c400002, ONE_OPERAND, nofunc },
    { "cogstop", 0x0c400003, ONE_OPERAND, nofunc },

    { "djnz",   0xe4800000, TWO_OPERANDS, nofunc },
    { "hubop",  0x0c000000, TWO_OPERANDS, nofunc },
    { "jmp",    0x5c000000, JMP_OPERAND, nofunc },
    { "jmpret", 0x5c800000, TWO_OPERANDS, nofunc },

    { "lockclr",0x0c400007, ONE_OPERAND, nofunc },
    { "locknew",0x0c400004, ONE_OPERAND, nofunc },
    { "lockret",0x0c400005, ONE_OPERAND, nofunc },
    { "lockset",0x0c400006, ONE_OPERAND, nofunc },

    { "max",    0x4c800000, TWO_OPERANDS, nofunc },
    { "maxs",   0x44800000, TWO_OPERANDS, nofunc },
    { "min",    0x48800000, TWO_OPERANDS, nofunc },
    { "mins",   0x40800000, TWO_OPERANDS, nofunc },
    { "mov",    0xa0800000, TWO_OPERANDS, nofunc },
    { "movd",   0x54800000, TWO_OPERANDS, nofunc },
    { "movi",   0x58800000, TWO_OPERANDS, nofunc },
    { "movs",   0x50800000, TWO_OPERANDS, nofunc },

    { "muxc",   0x70800000, TWO_OPERANDS, nofunc },
    { "muxnc",  0x74800000, TWO_OPERANDS, nofunc },
    { "muxz",   0x78800000, TWO_OPERANDS, nofunc },
    { "muxnz",  0x7c800000, TWO_OPERANDS, nofunc },

    { "neg",    0xa4800000, TWO_OPERANDS, nofunc },
    { "negc",   0xb0800000, TWO_OPERANDS, nofunc },
    { "negnc",  0xb4800000, TWO_OPERANDS, nofunc },
    { "negnz",  0xbc800000, TWO_OPERANDS, nofunc },
    { "negz",   0xb8800000, TWO_OPERANDS, nofunc },
    { "nop",    0x00000000, NO_OPERANDS, nofunc },

    { "or",     0x68800000, TWO_OPERANDS, nofunc },

    { "rcl",    0x34800000, TWO_OPERANDS, nofunc },
    { "rcr",    0x30800000, TWO_OPERANDS, nofunc },
    { "rdbyte", 0x00800000, TWO_OPERANDS, nofunc },
    { "rdlong", 0x08800000, TWO_OPERANDS, nofunc },
    { "rdword", 0x04800000, TWO_OPERANDS, nofunc },
    { "ret",    0x5c400000, NO_OPERANDS, nofunc },
    { "rev",    0x3c800000, TWO_OPERANDS, nofunc },
    { "rol",    0x24800000, TWO_OPERANDS, nofunc },
    { "ror",    0x20800000, TWO_OPERANDS, nofunc },

    { "sar",    0x38800000, TWO_OPERANDS, nofunc },
    { "shl",    0x2c800000, TWO_OPERANDS, nofunc },
    { "shr",    0x28800000, TWO_OPERANDS, nofunc },
    { "sub",    0x84800000, TWO_OPERANDS, nofunc },
    { "subs",   0xc0800000, TWO_OPERANDS, nofunc },
    { "subsx",  0xc4800000, TWO_OPERANDS, nofunc },
    { "subx",   0xcc800000, TWO_OPERANDS, nofunc },
    { "sumc",   0x90800000, TWO_OPERANDS, nofunc },
    { "sumnc",  0x94800000, TWO_OPERANDS, nofunc },
    { "sumnz",  0x9c800000, TWO_OPERANDS, nofunc },
    { "sumz",   0x98800000, TWO_OPERANDS, nofunc },

    { "test",   0x60000000, TWO_OPERANDS, nofunc },
    { "testn",  0x64000000, TWO_OPERANDS, nofunc },
    { "tjnz",   0xe8000000, TWO_OPERANDS, nofunc },
    { "tjz",    0xec000000, TWO_OPERANDS, nofunc },

    { "waitcnt", 0xf8800000, TWO_OPERANDS, nofunc },
    { "waitpeq", 0xf0800000, TWO_OPERANDS, nofunc },
    { "waitpne", 0xf4800000, TWO_OPERANDS, nofunc },
    { "waitvid", 0xfc800000, TWO_OPERANDS, nofunc },

    { "wrbyte", 0x00000000, TWO_OPERANDS, nofunc },
    { "wrlong", 0x08000000, TWO_OPERANDS, nofunc },
    { "wrword", 0x04000000, TWO_OPERANDS, nofunc },

    { "xor",    0x6c800000, TWO_OPERANDS, nofunc },
};

HwReg hwreg[] = {
    { "par", 0x1f0, "_PAR" },
    { "cnt", 0x1f1, "_CNT" },
    { "ina", 0x1f2, "_INA" },
    { "inb", 0x1f3, "_INB" },
    { "outa", 0x1f4, "_OUTA" },
    { "outb", 0x1f5, "_OUTB" },
    { "dira", 0x1f6, "_DIRA" },
    { "dirb", 0x1f7, "_DIRB" },
    { "ctra", 0x1f8, "_CTRA" },
    { "ctrb", 0x1f9, "_CTRB" },

    { "frqa", 0x1fa, "_FRQA" },
    { "frqb", 0x1fb, "_FRQB" },
    { "phsa", 0x1fc, "_PHSA" },
    { "phsb", 0x1fd, "_PHSB" },

    { "vcfg", 0x1fe, "_VCFG" },
    { "vscl", 0x1ff, "_VSCL" },

};

void
InitPasm(void)
{
    int i;

    /* add instructions */
    for (i = 0; i < N_ELEMENTS(instr); i++) {
        AddSymbol(&reservedWords, instr[i].name, SYM_INSTR, (void *)&instr[i]);
    }

    /* add hardware registers */
    for (i = 0; i < N_ELEMENTS(hwreg); i++) {
        AddSymbol(&reservedWords, hwreg[i].name, SYM_HWREG, (void *)&hwreg[i]);
    }
}

