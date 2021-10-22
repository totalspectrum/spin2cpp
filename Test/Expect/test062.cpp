#define __SPIN2CPP__
#include <propeller.h>
#include "test062.h"

#define Yield__() __asm__ volatile( "" ::: "memory" )
int32_t test062::Testit(int32_t X)
{
  Command = X;
  while (Command) {
    Yield__();
  }
  return Response;
}

