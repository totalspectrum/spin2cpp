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

/* flags for PrintExpr and friends */
#define PRINTEXPR_DEFAULT    0x0000
#define PRINTEXPR_GAS        0x0001  /* printing in a GAS context */
#define PRINTEXPR_ASSIGNMENT 0x0002  /* printing from an assignment operator */
#define PRINTEXPR_ISREF      0x0004  /* expression used as a reference */

/* printing functions */
void PrintExpr(Flexbuf *f, AST *expr, int flags);
void PrintExprToplevel(Flexbuf *f, AST *expr, int flags);
void PrintBoolExpr(Flexbuf *f, AST *expr, int flags);
void PrintAsAddr(Flexbuf *f, AST *expr, int flags);
void PrintExprList(Flexbuf *f, AST *list, int flags);
void PrintType(Flexbuf *f, AST *type);
void PrintPostfix(Flexbuf *f, AST *val, int toplevel, int flags);
void PrintInteger(Flexbuf *f, int32_t v);
void PrintFloat(Flexbuf *f, int32_t v);
int  PrintLookupArray(Flexbuf *f, AST *arr, int flags);
void PrintGasExpr(Flexbuf *f, AST *expr);
void PrintSymbol(Flexbuf *f, Symbol *sym, int flags);

#endif
