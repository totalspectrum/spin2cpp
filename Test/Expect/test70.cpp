#include <propeller.h>
#include "test70.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

uint8_t test70::dat[] = {
  0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64, 0x0a, 0x00, 0x00, 0x00, 
  0x34, 0x12, 0x00, 0x00, 
};
int32_t test70::Getx(void)
{
  int32_t result = 0;
  return (*(uint8_t *)&dat[0]);
}

