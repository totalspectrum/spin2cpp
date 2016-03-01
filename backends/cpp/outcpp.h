/*
 * Spin to C/C++ converter
 * Copyright 2011-2016 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for handling functions
 */
#include "cppfunc.h"

void PrintAnnotationList(FILE *f, AST *ast, char terminal);
void PrintIndentedComment(FILE *f, AST *ast, int indent);
void PrintDebugDirective(FILE *f, AST *ast);
void PrintNewline(FILE *f);
int PrintPublicFunctionDecls(FILE *f, Module *P);
int PrintPrivateFunctionDecls(FILE *f, Module *P);
void PrintFunctionBodies(FILE *f, Module *P);

/* printing functions */
void PrintExpr(FILE *f, AST *expr);
void PrintExprToplevel(FILE *f, AST *expr);
void PrintBoolExpr(FILE *f, AST *expr);
void PrintAsAddr(FILE *f, AST *expr);
void PrintExprList(FILE *f, AST *list);
void PrintType(FILE *f, AST *type);
void PrintPostfix(FILE *f, AST *val, int toplevel);
void PrintInteger(FILE *f, int32_t v);
void PrintFloat(FILE *f, int32_t v);
int  PrintLookupArray(FILE *f, AST *arr);
void PrintGasExpr(FILE *f, AST *expr);

