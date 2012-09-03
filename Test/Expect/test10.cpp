#include <propeller.h>
#include "test10.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test10::Test1(int32_t X, int32_t Y, int32_t Z)
{
  int32_t result = 0;
  return (X + (Y * Z));
}

int32_t test10::Test2(int32_t X, int32_t Y, int32_t Z)
{
  int32_t result = 0;
  return ((X + Y) * Z);
}

int32_t test10::Test3(int32_t X, int32_t Y, int32_t Z)
{
  int32_t result = 0;
  return ((X * Y) + Z);
}

