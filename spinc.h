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

/* declare a global variable if it does not already exist */
void MaybeDeclareGlobal(Module *P, AST *ident, AST *typ);

/* perform useful Spin specific transformations */
void SpinTransform(Module *Q);

/* ditto for BASIC */
void BasicTransform(Module *Q);

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

// type inference based on BASIC name (e.g. A$ is a string)
AST *InferTypeFromName(AST *identifier);

// declare a module level variable if one does not already exist
void MaybeDeclareGlobal(Module *P, AST *identifier, AST *typ);

// calculate number of expression items that may be placed on the stack
int NumExprItemsOnStack(AST *param);

// find the "main" function in a module, if any
Function *GetMainFunction(Module *P);

#endif
