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

/* perform useful Spin specific transformations */
void SpinTransform(Module *Q);

// top level functions
// parse a spin file
Module *ParseFile(const char *name);

// parse a top level spin file (resets global state)
Module *ParseTopFile(const char *name);

// process Spin functions (do type deduction, etc.)
// isBinary is non-zero if we are doing a final binary output
void ProcessSpinCode(Module *P, int isBinary);

// recursively assign offsets to all objects in modules
void AssignObjectOffsets(Module *P);

// type inference based on BASIC name (e.g. A$ is a string)
AST *InferTypeFromName(AST *identifier);

// declare a module level variable if one does not already exist
void MaybeDeclareGlobal(Module *P, AST *identifier, AST *typ);

#endif
