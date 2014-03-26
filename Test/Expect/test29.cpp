#include <propeller.h>
#include "test29.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define Yield__() __asm__ volatile( "" ::: "memory" )
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#undef lockclr /* work around a bug in propgcc */
#define lockclr(i) __asm__ volatile( "  lockclr %[_id]" : : [_id] "r"(i) :)
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#define Yield__()
#endif

int32_t test29::Tx(int32_t Val)
{
  ((uint8_t *)28672)[Idx] = 0;
  return 0;
}

int32_t test29::Str(int32_t Stringptr)
{
  //  Send string                    
  while (lockset(Strlock)) {
    Yield__();
  }
  {
    int32_t _idx__0000;
    int32_t _limit__0001 = strlen((char *) Stringptr);
    for(_idx__0000 = 1; _idx__0000 <= _limit__0001; (_idx__0000 = (_idx__0000 + 1))) {
      Tx(((uint8_t *)(Stringptr++))[0]);
    }
  }
  lockclr(Strlock);
  return 0;
}

