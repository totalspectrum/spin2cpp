#ifndef LEX_H
#define LEX_H

#include <stdio.h>
#include <stdbool.h>
#include "spintokens.h"
#include "util/flexbuf.h"

/*
 * input stream; could be either a string or a file
 */
typedef struct lexstream LexStream;

struct lexstream {
    void *ptr;  /* current pointer */
    void *arg;  /* original value of pointer */
    int (*getcf)(LexStream *);
#define UNGET_MAX 4 /* we can ungetc this many times */
    int ungot[UNGET_MAX];
    int ungot_ptr;

#define MAX_INDENTS 64
    /* input state */
    int in_block;  /* T_DAT, T_PUB, or T_CON */
    int save_block; /* so we can nest T_ASM inside others */
    int block_firstline; /* what line does the block start on? */
    int indent[MAX_INDENTS];    /* current indentation level */
    int indentsp;               /* pointer into stack of indentation level */
    int pending_indent;
    int eoln;      /* 1 if end of line seen */
    int eof;       /* 1 if EOF seen */
    int firstNonBlank;

    /* last global label */
    const char *lastGlobal;

    /* for error messages */
    int lineCounter;
    int colCounter;
    const char *fileName;

    /* for handling Unicode CR+LF */
    int sawCr;

    int pendingLine;  /* 1 if lineCounter needs incrementing */

    Flexbuf curLine;  /* string of current line */
    Flexbuf lines;    /* pointers to all lines in the file */
};

extern int lexgetc(LexStream *L);
extern void lexungetc(LexStream *L, int c);
extern void EstablishIndent(LexStream *L, int level);
extern void resetLineState(LexStream *L);

/*
 * function to initialize the lexer
 */
void initLexer(int flags);

/*
 * function to open a lexer stream from a string
 */
void strToLex(LexStream *lex, const char *s, const char *name);

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

/*
 * function to check for C reserved words
 */
bool Is_C_Reserved(const char *);

/*
 * function to convert an identifier to have upper case
 * first char and lower case remainder
 */
void NormalizeIdentifier(char *name);

/*
 * function to retrieve pending comments
 */
AST *GetComments(void);

#endif
