#include <propeller.h>
#include "test73.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

uint8_t test73::dat[] = {
  0x00, 0x60, 0x00, 0x00, 
};
int32_t test73::Flip(int32_t X)
{
  int32_t result = 0;
  OUTA = ((OUTA & 0xfffffff0) | 0xc);
  return (*(int32_t *)&dat[0]);
}

