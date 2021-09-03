/*
 * Spin to C/C++ converter
 * Copyright 2011-2018 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for handling functions
 */
#ifndef OUTCPP_H_
#define OUTCPP_H_

#include "util/flexbuf.h"
#include "becommon.h"
#include "cppfunc.h"

//
// back end data for modules
//
typedef struct asmmoddata {
    /* flags for emitting macros */
    char needsMinMax;
    char needsRotate;
    char needsShr;
    char needsStdlib;
    char needsString;
    char needsYield;
    char needsAbortdef;
    char needsRand;
    char needsSqrt;
    char needsLookup;
    char needsLookdown;
    char needsHighmult;
    char needsBitEncode;
    char needsLockFuncs;
    char needsCogAccess;
    char needsCoginit;

    /* flags for whether tuples of size N are needed */
    /* if needsTuple & (1<<N) then we need a definition for TupleN__ */
    uint32_t needsTuple;
    
} CppModData;

#define ModData(P) ((CppModData *)(P->bedata))


/* flags for PrintVarList and PrintType */
#define PUBLIC 0
#define PRIVATE 1
#define LOCAL 2
#define ISVOLATILE 4
#define ISCONST 8
int PrintVarList(Flexbuf *f, AST *typ, AST *list, int flags);

void PrintAssign(Flexbuf *f, AST *left, AST *right, int flags);

void PrintCommentString(Flexbuf *f, const char *str, int indent);
void PrintAnnotationList(Flexbuf *f, AST *ast, char terminal);
void PrintIndentedComment(Flexbuf *f, AST *ast, int indent);
void PrintDebugDirective(Flexbuf *f, AST *ast);
void PrintNewline(Flexbuf *f);
int PrintPublicFunctionDecls(Flexbuf *f, Module *P);
int PrintPrivateFunctionDecls(Flexbuf *f, Module *P);
void PrintFunctionBodies(Flexbuf *f, Module *P);

typedef struct cppInlineState {
    AST *outputs;
    AST *inputs;
    unsigned flags;
    int indent;
} CppInlineState;
void outputGasInstruction(Flexbuf *f, AST *ast, int inlineAsm, CppInlineState *state);

#endif
