#include <propeller.h>
#include "test67.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

uint8_t test67::dat[] = {
  0x3b, 0xaa, 0xb8, 0x3f, 
};
int32_t test67::Getx(void)
{
  int32_t result = 0;
  return (*(int32_t *)&dat[0]);
}

