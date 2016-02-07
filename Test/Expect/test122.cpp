#include <propeller.h>
#include "test122.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

void test122::Test(void)
{
  OUTA = ( Voidfunc(1), 0 );
}

void test122::Voidfunc(int32_t Y)
{
  DIRA = Y;
}

