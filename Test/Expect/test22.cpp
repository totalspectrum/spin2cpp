#include <propeller.h>
#include "test22.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test22::Calc(int32_t X, int32_t Y)
{
  int32_t R = 0;
  R = (X * Y);
  return R;
}

