#include <propeller.h>
#include "test38.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

INLINE__ int32_t Min__(int32_t a, int32_t b) { return a < b ? a : b; }
INLINE__ int32_t Max__(int32_t a, int32_t b) { return a > b ? a : b; }
int32_t test38::Big(int32_t X, int32_t Y)
{
  int32_t result = 0;
  result = (Max__(X, Y));
  return result;
}

int32_t test38::Small(int32_t X, int32_t Y)
{
  int32_t result = 0;
  result = (Min__(X, Y));
  return result;
}

