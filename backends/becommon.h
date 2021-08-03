#ifndef BACKEND_COMMON_H
#define BACKEND_COMMON_H

#include "ast.h"
#include "symbol.h"
#include "expr.h"

AST *BuildMethodPointer(AST *ast);
void OutputAlignLong(Flexbuf *fb);
void OutputDataBlob(Flexbuf *fb, Flexbuf *databuf, Flexbuf *relocbuf, const char *startLabel);

void NormalizeVarOffsets(Function *f);

void CompileAsmToBinary(const char *binname, const char *asmname); // in cmdline.c

// evaluate any constant expressions inside a string
AST *EvalStringConst(AST *expr);

// turn an AST stringptr into a flexbuf buffer
void StringBuildBuffer(Flexbuf *fb, AST *expr);

// print into a freshly allocated string
char *auto_printf(size_t max,const char *format,...) __attribute__((format(printf,2,3)));

#endif /* BACKEND_COMMON_H */
