#include <propeller.h>
#include "test108.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

void test108::Main(void)
{
  DIRA |= (255 << 16);
  Show(12);
  OUTA ^= 0xff0000;
}

void test108::Show(int32_t X)
{
  OUTA = (OUTA & 0xff00ffff) | ((X & 0xff) << 16);
}

