#include <sys/thread.h>
#include <propeller.h>
#include "test62.h"

#define Yield__() (__napuntil(_CNT + 1))
int32_t test62::testit(int32_t x)
{
  int32_t result = 0;
  command = x;
  while (command) {
    Yield__();
  }
  return response;
}

