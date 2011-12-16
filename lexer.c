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
#include "lexer.h"

int lineCounter = 1;

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
    L->arg = L->ptr = (void *)s;
    L->getcf = strgetc;
    L->ungetcf = strungetc;
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
parseIdentifier(LexStream *L, char **deststr)
{
    int c;
    char *place = NULL;
    size_t space = 0;
    size_t len = 0;
    c = lexgetc(L);
    while (isIdentifierChar(c)) {
        addchar(c, &place, &space, &len);
        c = lexgetc(L);
    }
    addchar('\0', &place, &space, &len);
    lexungetc(L, c);
    *deststr = place;
    return T_IDENTIFIER;
}

//
// skip over comments and spaces
// return first non-comment non-space character
//
int
skipSpace(LexStream *L)
{
    int c;
    int commentNest;

again:
    c = lexgetc(L);
    while (c == ' ' || c == '\t')
        c = lexgetc(L);
    if (c == '\'') {
        do {
            c = lexgetc(L);
        } while (c != '\n' && c != T_EOF);
    }
    if (c == '{') {
        commentNest = 1;
        do {
            c = lexgetc(L);
            if (c == '\n')
                lineCounter++;
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
        lineCounter++;
        goto again;
    }
    return c;
}

int
getToken(LexStream *L, TokenType *tok)
{
//    int base = 10;
    int c;

    c = skipSpace(L);

    if (isdigit(c)) {
        lexungetc(L,c);
        c = parseNumber(L, 10, &tok->ival);
    } else if (c == '$') {
        c = parseNumber(L, 16, &tok->ival);
    } else if (c == '%') {
        c = lexgetc(L);
        if (c == '%') {
            c = parseNumber(L, 4, &tok->ival);
        } else {
            lexungetc(L, c);
            c = parseNumber(L, 2, &tok->ival);
        }
    } else if (isIdentifierStart(c)) {
        lexungetc(L, c);
        c = parseIdentifier(L, &tok->string);
    }

    return c;
}
