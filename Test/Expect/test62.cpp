#include <propeller.h>
#include "test62.h"

#define Yield__() __asm__ volatile( "" ::: "memory" )
int32_t test62::Testit(int32_t X)
{
  Command = X;
  while (Command) {
    Yield__();
  }
  return Response;
}

