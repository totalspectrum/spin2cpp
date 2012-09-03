#include <propeller.h>
#include "test09.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test09::Init(void)
{
  int32_t result = 0;
  DIRA |= (1<<2);
  OUTA = ((OUTA & 0xffffff0f) | 0xa0);
  return result;
}

