/*
 * Spin to C/C++ converter
 * Copyright 2011-2024 Total Spectrum Software Inc.
 * main header file
 */

#ifndef SPINC_H
#define SPINC_H

#include <stdint.h>

#include "ast.h"
#include "frontends/common.h"
#include "frontends/lexer.h"

// definition for the Spin parser
#define SPINYYSTYPE AST*
#define BASICYYSTYPE AST*
#define CGRAMYYSTYPE AST*

extern Module *allparse;

/* printing functions */

extern int IsReservedWord(const char *str);

/* function to canonicalize an identifier */
void CanonicalizeIdentifier(char *idstr);

/* perform useful Spin specific transformations */
void SpinTransform(Function *f);

/* ditto for BASIC */
void BasicTransform(Function *f);

/* and C */
void CTransform(Function *f);

/* and BF */
void BFTransform(Function *f);

/* check and possibly convert types */
AST *CheckTypes(AST *);

/* helper */
void InitGlobalFuncs(void);

/* do some optimizations on a counting repeat loop */
AST *TransformCountRepeat(AST *);

// process a module after parsing it
void ProcessModule(Module *P);

// top level functions
// parse a spin file; params is optional list of overriden constants
Module *ParseFile(const char *name, AST *params);

// parse top level spin files (resets global state)
// outputBin is nonzero if we are outputting binary code
Module *ParseTopFiles(const char *argv[], int argc, int outputBin);

// calculate number of expression items that may be placed on the stack
int NumExprItemsOnStack(AST *param);

// find the "main" function in a module, if any
Function *GetMainFunction(Module *P);

// construct an AST corresponding to a DEBUG() statement
// see (frontends/printdebug.c)
AST *BuildDebugList(AST *exprlist, AST *dbgmask);

// convert AST debug list into printf
AST *CreatePrintfDebug(AST *exprlist, AST *dbgmask);

#endif
