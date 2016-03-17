/*
 * Spin to C/C++ converter
 * Copyright 2011-2016 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for handling functions
 */
#ifndef OUTCPP_H_
#define OUTCPP_H_

#include "util/flexbuf.h"
#include "cppfunc.h"

void PrintAnnotationList(Flexbuf *f, AST *ast, char terminal);
void PrintIndentedComment(Flexbuf *f, AST *ast, int indent);
void PrintDebugDirective(Flexbuf *f, AST *ast);
void PrintNewline(Flexbuf *f);
int PrintPublicFunctionDecls(Flexbuf *f, Module *P);
int PrintPrivateFunctionDecls(Flexbuf *f, Module *P);
void PrintFunctionBodies(Flexbuf *f, Module *P);

/* printing functions */
void PrintExpr(Flexbuf *f, AST *expr);
void PrintExprToplevel(Flexbuf *f, AST *expr);
void PrintBoolExpr(Flexbuf *f, AST *expr);
void PrintAsAddr(Flexbuf *f, AST *expr);
void PrintExprList(Flexbuf *f, AST *list);
void PrintType(Flexbuf *f, AST *type);
void PrintPostfix(Flexbuf *f, AST *val, int toplevel);
void PrintInteger(Flexbuf *f, int32_t v);
void PrintFloat(Flexbuf *f, int32_t v);
int  PrintLookupArray(Flexbuf *f, AST *arr);
void PrintGasExpr(Flexbuf *f, AST *expr);

/* fetch clock frequency settings */
int GetClkFreq(Module *P, unsigned int *clkfreqptr, unsigned int *clkregptr);

#endif
