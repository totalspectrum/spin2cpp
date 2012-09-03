#include <propeller.h>
#include "test95.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif


//
// and here are the C versions of the methods
//
int32_t test95::Square(int32_t x)
{
  return x*x;
}


