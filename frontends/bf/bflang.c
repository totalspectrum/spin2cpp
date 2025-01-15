/*
 * BF to C or PASM translater
 * Copyright 2025 Total Spectrum Software and Collabora, Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * A parser for a really simple language to the flexspin internal API
 * Intended more as an example for implementors than as something
 * to actually use in the real world
 *
 * The language implemented here is an esoteric language called
 * BrainF*ck or BF (or various other things) with only 8 commands,
 * each consisting of only a single character:
 *   >  Increment data pointer by 1
 *   <  Decrement data pointer by 1
 *   +  Increment the byte at the data pointer by 1
 *   -  Decrement the byte at the data pointer by 1
 *   .  Output the byte at the data pointer
 *   ,  Read a byte and store it at the data pointer
 *   [  If the byte at the data pointer is 0, jump to the command
 *      after the next matching ]
 *   ]  If the byte at the data pointer is non-zero, jump back to
 *      the previous matching [
 *
 * All other characters are ignored and considered to be comments.
 *
 * I/O is performed using the standard Spin SEND and RECV pointers, so
 * a BF program may be included as an object in a Spin program. If
 * SEND and/or RECV are not initialized, they are set to an appropriate serial
 * function.
 */

#include <stdlib.h>
#include <string.h>
#include "spinc.h"

static AST *array_name;     /* the name of the BF main BF data array */
static AST *array_index;    /* the name of the var to index that array */
static AST *array_mask;     /* a mask so we wrap around at end of array */
static unsigned array_size; /* size of the array (based on processor) */

static AST *cur_pos_deref;  /* a dereference to fetch/store the byte at the current position */

/*
 * fetch a single BF character
 * ignore any non-useful (comment) characters
 * returns -1 on EOF
 */
static int nextbfchar(LexStream *L) {
    int c;

    do {
        c = lexgetc(L);
        switch (c) {
        case '[':
        case ']':
        case '+':
        case '-':
        case '<':
        case '>':
        case '.':
        case ',':
            /* a valid BF character, return to parser */
            return c;
        default:
            break;
        }
    } while (c > 0);
    return c;
}

/*
 * add an expression wrapped in a statement list to
 * a list of AST's
 * returns the new value for the list
 */
static AST *
append_expr_statement(AST *body, AST *expr)
{
    AST *stmt = NewAST(AST_STMTLIST, expr, NULL);
    body = AddToList(body, stmt);
    return body;
}

/*
 * Utility routine: read a sequence of characters ch,
 * count them, and then increment or decrement the
 * given variable reference by that many.
 * Optionally (if mask is non-NULL) applies the given
 * mask before storing the result back.
 */
static AST *
incDecVar(LexStream *L, int ch, int incDecOp, AST *varref, AST *mask)
{
    int n = 0;
    int c = ch;
    AST *ast;
    // count how many ch's in a row we see
    // strictly speaking we don't have to merge them like this,
    // but it is a tremendous optimization
    while (c == ch) {
        n++;
        c = nextbfchar(L);
    }
    // the last character we saw was not '+', so
    // save it back for the next parse
    lexungetc(L, c);

    ast = AstOperator(incDecOp, varref, AstInteger(n));
    if (mask) {
        ast = AstOperator('&', ast, mask);
    }
    // create the statement to update the variable
    ast = AstAssign(varref, ast);
    return ast;
}

/*
 * return a dereference of the current position
 */
static AST *CurPos(void) {
    return DupAST(cur_pos_deref);
}

/*
 * parse a BF expression; this can be one of:
 * .: output current character
 * ,: read a character and write to current location
 * a sequence of N +'s: add N to current location
 * a sequence of N -'s: subtract N from current location
 * a sequence of N >'s: add N to current location pointer
 * a sequence of N <'s: subtract N from current location pointer
 * [: form a loop up until the next ]
 * ]: terminate a loop
 *
 * Returns an AST for the expression, or NULL
 * on EOF
 */
static AST *
parseBFstream(LexStream *L)
{
    int c;
    AST *loopbody = NULL;
    AST *ast;

    for(;;) {
        c = nextbfchar(L);
        if (c == '+') {
            ast = incDecVar(L, c, '+', CurPos(), NULL);
        } else if (c == '-') {
            ast = incDecVar(L, c, '-', CurPos(), NULL);
        } else if (c == '>') {
            ast = incDecVar(L, c, '+', array_index, array_mask);
        } else if (c == '<') {
            ast = incDecVar(L, c, '-', array_index, array_mask);
        } else if (c == '.') {
            // print the byte at the current position
            AST *sendptr = AstIdentifier("__sendptr");
            ast = NewAST(AST_FUNCCALL,
                         sendptr,
                         NewAST(AST_EXPRLIST, CurPos(), NULL));
        } else if (c == ',') {
            // read something into the current position
            AST *recvptr = AstIdentifier("__recvptr");
            ast = AstAssign(CurPos(),
                            NewAST(AST_FUNCCALL,
                                   recvptr,
                                   NULL));
        } else if (c == '[') {
            // open a loop
            // this should emit
            // if (*cur_pos_deref != 0) {
            //   do {
            //      <loopbody>
            //   } while (*cur_pos_deref != 0);
            ast = parseBFstream(L);
            // create the do-while loop
            ast = NewAST(AST_DOWHILE,
                         AstOperator(K_NE, CurPos(), AstInteger(0)),
                         ast);
            // wrap it in the if
            ast = NewAST(AST_IF,
                         AstOperator(K_NE, CurPos(), AstInteger(0)),
                         NewAST(AST_THENELSE,
                                NewAST(AST_STMTLIST, ast, NULL),
                                NULL));
        } else if (c == ']') {
            ast = NULL;
            break;
        } else {
            if (c != -1) {
                ERROR(NULL, "unexpected character encountered");
            }
            break;
        }
        loopbody = AddToList(loopbody,
                             NewAST(AST_STMTLIST, ast, NULL));
    }   
    return loopbody;
}

/*
 * initialize a BF parse
 * there will be only one function (the
 * main body) which will go in current->body
 * we need to create an array (8K for P1, 32K for P2)
 * Note that the parser is not re-entrant (none of
 * flexspin's parsers are) so we're using static variables
 * for everything
 */

static void
init_bf_parse(LexStream *L)
{
    AST *ast;
    
    // array size is 8K for P1 (too small, but the P1 only has 32K total)
    // and 32K for P2
    // must be a power of 2 so array_mask can work
    array_size = gl_p2 ? 32768 : 8192;

    // create an array mask
    array_mask = AstInteger(array_size - 1);

    // create the member variable to hold the BF data (an array of bytes)
    // this declaration is like
    //     long bf_array[array_size]
    // in C
    array_name = AstIdentifier("bf_array");
    AST *array_decl = NewAST(AST_ARRAYDECL,
                             array_name, AstInteger(array_size));
    // the base index of the array is put in d.ptr (it will default to 0,
    // but it doesn't hurt to make it explicit)
    array_decl->d.ptr = AstInteger(0);

    // we put the array in the module's DAT section
    MaybeDeclareMemberVar(current, array_decl, ast_type_byte, 0, NORMAL_VAR);
    
    // create a local variable declaration for the array_index
    array_index = AstIdentifier("bf_pos");
    // declare it and initialize it to 0
    AST *initZero = AstAssign(array_index, AstInteger(0));

    // the variable declaration AST expects a list of variables to
    // initialize, hence the AST_LISTHOLDER
    AST *decl = NewAST(AST_DECLARE_VAR,
                       ast_type_long,
                       NewAST(AST_LISTHOLDER, initZero, NULL));

    // add the variable declaration to the main program body
    current->body = append_expr_statement(current->body, decl);

    // create a dereference bf_array[bf_pos]
    cur_pos_deref = NewAST(AST_ARRAYREF,
                           array_name, array_index);

    // This part below is somewhat technical; we want to do I/O by using
    // the standard Spin2 send() and recv() functions. But those may not
    // be initialized (if we're not using the BF program as an object).
    // So add some code to initialize them to the default _tx and _rx
    // functions
    AST *sendfunc = AstIdentifier("__sendptr");
    AST *recvfunc = AstIdentifier("__recvptr");
    
    // check for sendfunc initialized
    ast = AstOperator(K_EQ, sendfunc, AstInteger(0));
    ast = NewAST(AST_IF, ast,
                 NewAST(AST_THENELSE,
                        NewAST(AST_STMTLIST,
                               AstAssign(sendfunc,
                                         NewAST(AST_ADDROF,
                                                AstIdentifier("_tx"),
                                                NULL)),
                               NULL),
                        NULL));
    current->body = append_expr_statement(current->body, ast);

    // check for recvfunc initialized
    ast = AstOperator(K_EQ, recvfunc, AstInteger(0));
    ast = NewAST(AST_IF, ast,
                 NewAST(AST_THENELSE,
                        NewAST(AST_STMTLIST,
                               AstAssign(recvfunc,
                                         NewAST(AST_ADDROF,
                                                AstIdentifier("_rx"),
                                                NULL)), NULL),
                        NULL));
    current->body = append_expr_statement(current->body, ast);
    
}

/*
 * This uses the same prototype as a YACC generated parser, so it takes no
 * parameters and uses the lexer stream in the current module (a global
 * variable. This is ugly, but compatible.
 * returns 0 on success, -1 on failure
 */
int bfparse(void)
{
    LexStream *L = current->Lptr;
    AST *ast;
    int c;
    
    /* initialize the parser state */
    init_bf_parse(L);

    ast = parseBFstream(L);
    current->body = AddToList(current->body, ast);

    // we should be at EOF here
    c = lexpeekc(L);
    if (c > 0) {
        ERROR(NULL, "[] mismatch detected");
        return -1;
    }
    return 0;
}

//
// High level transformation on the intermediate AST
// For BF, this doesn't really have to do anything, but
// we may as well run the standard high level optimizations
// and such
//
void
BFTransform(Function *func)
{
    InitGlobalFuncs();

    // simplify assignments
    DoHLTransforms(func);

    // Do type checking here. For BF this is
    // only needed because of the setup of send := @_tx
    // at initialization time
    CheckTypes(func->body);
}
