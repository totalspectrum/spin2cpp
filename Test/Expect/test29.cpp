#include <propeller.h>
#include "test29.h"

#define Yield__() __asm__ volatile( "" ::: "memory" )
void test29::Tx(int32_t Val)
{
  ((uint8_t *)28672)[Idx] = 0;
}

void test29::Str(int32_t Stringptr)
{
  int32_t	_idx__0000, _limit__0001;
  // Send string                    
  while (lockset(Strlock)) {
    Yield__();
  }
  for(( (_idx__0000 = 0), (_limit__0001 = strlen((char *) Stringptr)) ); _idx__0000 < _limit__0001; _idx__0000++) {
    Tx(((uint8_t *)(Stringptr++))[0]);
  }
  lockclr(Strlock);
}

