#include <string.h>
#define __SPIN2CPP__
#include <propeller.h>
#include "test029.h"

#define Yield__() __asm__ volatile( "" ::: "memory" )
void test029::Tx(int32_t Val)
{
  ((char *)28672)[Idx] = 0;
}

void test029::Str(int32_t Stringptr)
{
  int32_t 	_idx__0000;
  // Send string                    
  while (lockset(Strlock)) {
    Yield__();
  }
  for(_idx__0000 = strlen((const char *)Stringptr); _idx__0000 != 0; --_idx__0000) {
    Tx(((char *)(Stringptr++))[0]);
  }
  lockclr(Strlock);
}

