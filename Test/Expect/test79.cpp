#include <propeller.h>
#include "test79.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test79::I2c_start(void)
{
  int32_t result = 0;
  OUTA = ((OUTA & 0xefffffff) | 0x10000000);
  DIRA = ((DIRA & 0xefffffff) | 0x10000000);
  OUTA = ((OUTA & 0xdfffffff) | 0x20000000);
  DIRA = ((DIRA & 0xdfffffff) | 0x20000000);
  OUTA &= ~(1<<29);
  OUTA &= ~(1<<28);
  return result;
}

