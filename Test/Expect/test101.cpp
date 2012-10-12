#include <propeller.h>
#include "test101.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test101::Foo(void)
{
  int32_t result = 0;
  OUTA = (int32_t)0xbf800000;
  return result;
}

