#include <propeller.h>
#include "test69.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test69::Demo(void)
{
  int32_t result = 0;
  if (!(((_INA >> 0) & 0x1))) {
    abort();
  } else {
    if (!(((_INA >> 1) & 0x1))) {
      _OUTA &= ~(7<<0);
    }
  }
  return result;
}

