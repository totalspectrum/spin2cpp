#include <propeller.h>
#include "test103.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test103::Blah(int32_t X, int32_t Y)
{
  int32_t result = 0;
  return -(X >= Y);
}

int32_t test103::Foo(int32_t X, int32_t Y)
{
  int32_t result = 0;
  return -(X <= Y);
}

int32_t test103::Blah2(int32_t X)
{
  int32_t result = 0;
  J = -(J >= X);
  return J;
}

