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

    { ":=", T_ASSIGN },
    { "..", T_DOTS },
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
