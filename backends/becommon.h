#ifndef BACKEND_COMMON_H
#define BACKEND_COMMON_H

#include "ast.h"
#include "symbol.h"
#include "expr.h"

AST *BuildMethodPointer(AST *ast);
void OutputDataBlob(Flexbuf *fb, Flexbuf *databuf, Flexbuf *relocbuf, const char *startLabel);


#endif /* BACKEND_COMMON_H */
