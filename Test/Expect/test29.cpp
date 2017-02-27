#include <propeller.h>
#include "test29.h"

#define Yield__() __asm__ volatile( "" ::: "memory" )
void test29::Tx(int32_t Val)
{
  ((char *)28672)[Idx] = 0;
}

void test29::Str(int32_t Stringptr)
{
  int32_t 	_idx__0001, _limit__0002;
  // Send string                    
  while (lockset(Strlock)) {
    Yield__();
  }
  for(( (_idx__0001 = 0), (_limit__0002 = strlen((const char *)Stringptr)) ); _idx__0001 < _limit__0002; _idx__0001++) {
    Tx(((char *)(Stringptr++))[0]);
  }
  lockclr(Strlock);
}

