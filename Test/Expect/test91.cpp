#include <propeller.h>
#include "test91.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

__NATIVE
int32_t test91::Nativefunc(int32_t I)
{
  int32_t result = 0;
  return (I + 1);
}

