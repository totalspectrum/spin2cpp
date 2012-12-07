#include <propeller.h>
#include "test79.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test79::I2c_start(void)
{
  OUTA = ((OUTA & 0xefffffff) | 0x10000000);
  DIRA = ((DIRA & 0xefffffff) | 0x10000000);
  OUTA = ((OUTA & 0xdfffffff) | 0x20000000);
  DIRA = ((DIRA & 0xdfffffff) | 0x20000000);
  OUTA &= ~(1<<Sda);
  OUTA &= ~(1<<Scl);
  return 0;
}

