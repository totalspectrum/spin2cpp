#include <propeller.h>
#include "test71.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test71::Blah(void)
{
  int32_t _parm__0000[10];
  _parm__0000[0] = 0;
  for(_parm__0000[1] = 0; _parm__0000[1] <= 7; _parm__0000[1]++) {
    Foo((int32_t)(&_parm__0000[0]), (int32_t)(&_parm__0000[2 + _parm__0000[1]]));
  }
  return _parm__0000[0];
}

void test71::Foo(int32_t M, int32_t N)
{
  OUTA = OUTA | (1 << N);
}

