#include <propeller.h>
#include "test10.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

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

