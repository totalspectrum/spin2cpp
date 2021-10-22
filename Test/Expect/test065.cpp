#define __SPIN2CPP__
#include <propeller.h>
#include "test065.h"

#define Yield__() __asm__ volatile( "" ::: "memory" )
void test065::Dofunc(int32_t X)
{
  do {
    Yield__();
  } while (X);
}

