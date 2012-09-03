#include <propeller.h>
#include "test83.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

uint8_t test83::dat[] = {
  0x2d, 0x2d, 0x0d, 0x00, 0x20, 0x21, 0x00, 0x00, 0xf0, 0x03, 0xbc, 0xa0, 0x44, 0x33, 0x22, 0x11, 
};
int32_t test83::Start(void)
{
  int32_t result = 0;
  cognew((int32_t)(&(*(int32_t *)&dat[8])), 0);
  return result;
}

