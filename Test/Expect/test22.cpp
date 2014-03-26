#include <propeller.h>
#include "test22.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test22::Calc(int32_t X, int32_t Y)
{
  int32_t R = 0;
  // 
  //  just a comment
  // 
  R = (X * Y);
  return R;
}

