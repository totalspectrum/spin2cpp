
#include "spinc.h"

Flexbuf CompressExecutable(const char *inPtr, int inSize);
size_t CompileStub(Flexbuf *f, const char *moduleName, const char* srcPtr, size_t srcLen);
