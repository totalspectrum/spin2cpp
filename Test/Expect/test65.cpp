#include <propeller.h>
#include "test65.h"

#define Yield__() __asm__ volatile( "" ::: "memory" )
void test65::Dofunc(int32_t X)
{
  do {
    Yield__();
  } while (X);
}

