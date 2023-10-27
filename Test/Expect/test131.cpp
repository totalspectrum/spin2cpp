#include <stdlib.h>
#define __SPIN2CPP__
#include <propeller.h>
#include "test131.h"

extern "C" {
#include <setjmp.h>
}
typedef struct { jmp_buf jmp; int32_t val; } AbortHook__;
AbortHook__ *abortChain__ __attribute__((common));

unsigned char test131::dat[] = {
  0x00, 0x00, 0x80, 0x45, 0xcd, 0xcc, 0x8c, 0x45, 0x9a, 0x11, 0x00, 0x00, 0x99, 0x11, 0x00, 0x00, 
};
int32_t test131::Demo(void)
{
  if (!abortChain__) abort();
  abortChain__->val =  0;
  longjmp(abortChain__->jmp, 1);
  return 0;
}

