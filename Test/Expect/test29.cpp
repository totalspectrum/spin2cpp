#include <sys/thread.h>
#include <propeller.h>
#include "test29.h"

#define Yield__() (__napuntil(_CNT))
int32_t test29::Tx(int32_t Val)
{
  int32_t result = 0;
  ((uint8_t *)28672)[Idx] = 0;
  return result;
}

int32_t test29::Str(int32_t Stringptr)
{
  int32_t result = 0;
  while (lockset(Strlock)) {
    Yield__();
  }
  int32_t _idx__0000;
  _idx__0000 = strlen((char *) Stringptr);
  do {
    Tx(((uint8_t *)(Stringptr++))[0]);
    _idx__0000 = (_idx__0000 + -1);
  } while (_idx__0000 >= 1);
  lockclr(Strlock);
  return result;
}

