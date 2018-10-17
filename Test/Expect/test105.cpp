#include <stdlib.h>
#include <propeller.h>
#include "test105.h"

extern "C" {
#include <setjmp.h>
}
typedef struct { jmp_buf jmp; int32_t val; } AbortHook__;
AbortHook__ *abortChain__ __attribute__((common));

int32_t test105::Go(void)
{
  int32_t 	X;
  X = __extension__({ AbortHook__ *stack__ = abortChain__, here__; int32_t tmp__; abortChain__ = &here__; tmp__ = ((setjmp(abortChain__->jmp) == 0) ? Start() : abortChain__->val); abortChain__ = stack__; tmp__; });
  return X;
}

int32_t test105::Start(void)
{
  if (!abortChain__) abort();
  abortChain__->val =  5;
  longjmp(abortChain__->jmp, 1);
  return 0;
}

