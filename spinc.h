/*
 * main header file
 */

#ifndef SPINC_H
#define SPINC_H

/* Yacc define */
/* we need to put it up here because the lexer includes spin.tab.h */
#define YYSTYPE AST *

#include "ast.h"
#include "lexer.h"
#include "symbol.h"

/* parser state structure */
typedef struct parserstate {
    struct parserstate *next;  /* to make a stack */
    /* top level objects */
    AST *conblock;
    AST *functions;
    AST *datblock;
    AST *varblock;

    /* AST for current token */
    AST *ast;

    /* lexer state */
    LexStream L;

    /* the symbol table */
    SymbolTable objsyms;

    /* various file name related strings */
    char *basename;    /* the file name without ".spin" */
    char *classname;   /* the class name */
} ParserState;

/* the current parser state */
extern ParserState *current;

/* printing functions */
void PrintExpr(FILE *f, AST *expr);

/* evaluate a constant expression */
long EvalConstExpr(AST *expr);

/* code for printing errors */
extern int gl_errors;
void ERROR(const char *msg, ...);

#endif
