#ifndef LEX_H
#define LEX_H

#include <stdio.h>
#include <stdbool.h>
#include "optokens.h"
#include "util/flexbuf.h"

extern int lexgetc(LexStream *L);
extern void lexungetc(LexStream *L, int c);
extern void EstablishIndent(LexStream *L, int level);
extern void resetLineState(LexStream *L);

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
int getSpinToken(LexStream *, AST **ast);

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
