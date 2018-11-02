/*
 * Spin to C/C++ converter
 * Copyright 2011-2018 Total Spectrum Software Inc.
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

extern struct preprocess gl_pp;

extern Module *allparse;

/* printing functions */

/* declare constants */
void DeclareConstants(AST **conlist);

/* declare all functions */
void DeclareFunctions(Module *);

extern int IsReservedWord(const char *str);

/* function to canonicalize an identifier */
void CanonicalizeIdentifier(char *idstr);

/* perform useful Spin specific transformations */
void SpinTransform(Function *f);

/* ditto for BASIC */
void BasicTransform(Function *f);

/* and C */
void CTransform(Function *f);

/* check and possibly convert types */
AST *CheckTypes(AST *);

/* helper */
void InitGlobalFuncs(void);

/* do some optimizations on a counting repeat loop */
AST *TransformCountRepeat(AST *);

// top level functions
// parse a spin file
Module *ParseFile(const char *name);

// parse a top level spin file (resets global state)
// outputBin is nonzero if we are outputting binary code
Module *ParseTopFile(const char *name, int outputBin);

// recursively assign offsets to all objects in modules
void AssignObjectOffsets(Module *P);

// calculate number of expression items that may be placed on the stack
int NumExprItemsOnStack(AST *param);

// find the "main" function in a module, if any
Function *GetMainFunction(Module *P);

#endif
