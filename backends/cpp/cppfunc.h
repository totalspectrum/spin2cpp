#ifndef CPPFUNC_H_
#define CPPFUNC_H_

int PrintPublicFunctionDecls(FILE *f, Module *parse);
int PrintPrivateFunctionDecls(FILE *f, Module *parse);
void PrintFunctionBodies(FILE *f, Module *parse);

#endif
