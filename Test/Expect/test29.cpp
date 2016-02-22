#include <propeller.h>
#include "test29.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define Yield__() __asm__ volatile( "" ::: "memory" )
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#define Yield__()
#endif

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

