#ifndef LEX_H
#define LEX_H

#include <stdio.h>
#include "spin.tab.h"

/*
 * input stream; could be either a string or a file
 */
typedef struct lexstream LexStream;

struct lexstream {
    void *ptr;  /* current pointer */
    void *arg;  /* original value of pointer */
    int (*getcf)(LexStream *);
    void (*ungetcf)(LexStream *, int c);

#define MAX_INDENTS 64
    /* input state */
    int in_block;  /* T_DAT, T_PUB, or T_CON */
    int indent[MAX_INDENTS];    /* current indentation level */
    int indentsp;               /* pointer into stack of indentation level */
    int emit_outdents;          /* how many T_OUTDENTS to emit */
    int eoln;      /* 1 if end of line seen */

    /* last global label */
    const char *lastGlobal;

    /* for error messages */
    int lineCounter;
    const char *fileName;
};

#define lexgetc(p) ((p)->getcf((p)))
#define lexungetc(p, c) ((p)->ungetcf((p),(c)))

/*
 * function to initialize the lexer
 */
void initLexer(void);

/*
 * function to open a lexer stream from a string
 */
void strToLex(LexStream *lex, const char *s);

/*
 * function to open a lexer stream from a FILE
 */
void fileToLex(LexStream *lex, FILE *f, const char *name);

/*
 * function to get the next token from a stream
 * it will allocate and fill in an AST if necessary,
 * otherwise *ast will be set to NULL
 */
int getToken(LexStream *, AST **ast);

#endif
