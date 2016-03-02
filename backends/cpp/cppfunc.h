#ifndef CPPFUNC_H_
#define CPPFUNC_H_

int PrintPublicFunctionDecls(Flexbuf *f, Module *parse);
int PrintPrivateFunctionDecls(Flexbuf *f, Module *parse);
void PrintFunctionBodies(Flexbuf *f, Module *parse);

#endif
