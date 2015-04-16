#include <propeller.h>
#include "gtest01.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

#line 4 "gtest01.spin"
int32_t gtest01::foo(void)
{
#line 5 "gtest01.spin"
  if (x < 2) {
    return 0;
  } else {
    return 1;
  }
}

