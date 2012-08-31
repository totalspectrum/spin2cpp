#include <propeller.h>
#include "test75.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))
#endif

int32_t test75::Start(int32_t Code)
{
  int32_t _local__0000[4];
  int32_t result = 0;
  cognew(Code, (int32_t)(&_local__0000[0]));
  return result;
}

