#include <propeller.h>
#include "test106.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

uint8_t volatile test106::dat[] = {
  0x00, 0x00, 0x00, 0x00, 
};
int32_t test106::Foo(void)
{
  return ((*(int32_t *)&dat[0]) + 1);
}

