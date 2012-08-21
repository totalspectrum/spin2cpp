#include <propeller.h>
#include "test24.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test24::Unlock(void)
{
  int32_t result = 0;
  return PostEffect__(X, 0);
}

