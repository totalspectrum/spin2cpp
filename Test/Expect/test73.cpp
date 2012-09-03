#include <propeller.h>
#include "test73.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

uint8_t test73::dat[] = {
  0x00, 0x60, 0x00, 0x00, 
};
int32_t test73::Flip(int32_t X)
{
  int32_t result = 0;
  OUTA = ((OUTA & 0xfffffff0) | 0xc);
  return (*(int32_t *)&dat[0]);
}

