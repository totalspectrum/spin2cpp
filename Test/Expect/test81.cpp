#include <propeller.h>
#include "test81.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test81::High8(void)
{
  int32_t result = 0;
  OUTA |= (1<<8);
  DIRA |= (1<<8);
  return result;
}

int32_t test81::High(int32_t Pin)
{
  int32_t result = 0;
  OUTA |= (1<<Pin);
  DIRA |= (1<<Pin);
  return result;
}

