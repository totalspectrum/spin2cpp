#include <stdlib.h>
#define __SPIN2CPP__
#include <propeller.h>
#include "test072.h"

extern "C" {
#include <setjmp.h>
}
typedef struct { jmp_buf jmp; int32_t val; } AbortHook__;
AbortHook__ *abortChain__ __attribute__((common));

int32_t test072::Top(int32_t X)
{
  return __extension__({ AbortHook__ *stack__ = abortChain__, here__; int32_t tmp__; abortChain__ = &here__; tmp__ = ((setjmp(abortChain__->jmp) == 0) ? Sub1(X) : abortChain__->val); abortChain__ = stack__; tmp__; });
}

int32_t test072::Sub1(int32_t X)
{
  return (2 * Sub2(X));
}

int32_t test072::Sub2(int32_t X)
{
  if (X == 3) {
    // weird case
    if (!abortChain__) abort();
    abortChain__->val =  99;
    longjmp(abortChain__->jmp, 1);
  }
  return (X + 1);
}

