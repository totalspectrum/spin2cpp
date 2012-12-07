#include <propeller.h>
#include "test18.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

uint8_t test18::dat[] = {
  0xf1, 0x05, 0xbc, 0xa0, 0x01, 0x00, 0x7c, 0x5c, 0x01, 0x00, 0x00, 0x00, 
};
int32_t test18::Start(void)
{
  return (int32_t)(&(*(int32_t *)&dat[0]));
}

