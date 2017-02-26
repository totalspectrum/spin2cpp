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

/* flags for PrintVarList and PrintType */
#define PUBLIC 0
#define PRIVATE 1
#define LOCAL 2
#define VOLATILE 4
int PrintVarList(Flexbuf *f, int siz, AST *list, int flags);

void PrintAssign(Flexbuf *f, AST *left, AST *right, int flags);

void PrintCommentString(Flexbuf *f, const char *str, int indent);
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
#define PRINTEXPR_GASIMM     0x0008  /* GAS expression is an immediate value (so divide labels by 4) */
#define PRINTEXPR_GASOP      0x0010  /* GAS expression used in an operand */
#define PRINTEXPR_GASABS     0x0020  /* absolute address, not relative */
#define PRINTEXPR_USECONST   0x0040  /* print constant names, not values */
#define PRINTEXPR_TOPLEVEL   0x0080  /* leave out parens around operators */
#define PRINTEXPR_USEFLOATS  0x0100  /* print  expression as floats if appropriate */

/* printing functions */
void PrintTypedExpr(Flexbuf *f, AST *casttype, AST *expr, int flags);
void PrintExpr(Flexbuf *f, AST *expr, int flags);
void PrintBoolExpr(Flexbuf *f, AST *expr, int flags);
void PrintAsAddr(Flexbuf *f, AST *expr, int flags);
void PrintExprList(Flexbuf *f, AST *list, int flags, Function *func);
void PrintType(Flexbuf *f, AST *type, int flags);
void PrintCastType(Flexbuf *f, AST *type);
void PrintPostfix(Flexbuf *f, AST *val, int toplevel, int flags);
void PrintInteger(Flexbuf *f, int32_t v, int flags);
void PrintFloat(Flexbuf *f, int32_t v, int flags);
int  PrintLookupArray(Flexbuf *f, AST *arr, int flags);
void PrintGasExpr(Flexbuf *f, AST *expr, bool useFloat);
void PrintSymbol(Flexbuf *f, Symbol *sym, int flags);

#endif
