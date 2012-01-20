#include <sys/thread.h>
#include <propeller.h>
#include "test62.h"

#define Yield__() (__napuntil(_CNT + 1))
int32_t test62::Testit(int32_t X)
{
  int32_t result = 0;
  Command = X;
  while (Command) {
    Yield__();
  }
  return Response;
}

