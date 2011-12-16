#ifndef LEX_H
#define LEX_H

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
};

#define lexgetc(p) ((p)->getcf((p)))
#define lexungetc(p, c) ((p)->ungetcf((p),(c)))

/*
 * token struct
 */
typedef YYSTYPE TokenType;


/*
 * function to open a lexer stream from a string
 */
void strToLex(LexStream *lex, const char *s);

/*
 * function to get the next token from a stream
 */
int getToken(LexStream *, TokenType *tok);

#endif
