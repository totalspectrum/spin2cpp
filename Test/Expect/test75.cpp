#include <propeller.h>
#include "test75.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test75::Start(int32_t Code)
{
  int32_t _local__0000[4];
  cognew(Code, (int32_t)(&_local__0000[0]));
  return 0;
}

