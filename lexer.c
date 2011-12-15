//
// Simple lexical analyzer for a language where indentation
// is significant
//
// Copyright (c) 2011 Total Spectrum Software Inc.
//
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "lexer.h"


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

void strToLex(LexStream *L, const char *s)
{
    L->arg = L->ptr = (void *)s;
    L->getcf = strgetc;
    L->ungetcf = strungetc;
}

/*
 * actual parsing functions
 */
static int
parseNumber(LexStream *L, unsigned int base, unsigned long *num)
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

int
getToken(LexStream *L, TokenType *tok)
{
//    int base = 10;
    int c;

    c = lexgetc(L);
    while (c == ' ' || c == '\t')
        c = lexgetc(L);
    if (isdigit(c)) {
        lexungetc(L,c);
        c = parseNumber(L, 10, &tok->val);
    } else if (c == '$') {
        c = parseNumber(L, 16, &tok->val);
    } else if (c == '%') {
        c = lexgetc(L);
        if (c == '%') {
            c = parseNumber(L, 4, &tok->val);
        } else {
            lexungetc(L, c);
            c = parseNumber(L, 2, &tok->val);
        }
    }

    return c;
}
