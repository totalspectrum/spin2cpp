#include <propeller.h>
#include "test50.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

void test50::Myinit(int32_t A, int32_t B, int32_t C)
{
  int32_t _parm__0000[3];
  _parm__0000[0] = A;
  _parm__0000[1] = B;
  _parm__0000[2] = C;
  memmove( (void *)&Gb, (void *)&_parm__0000[1], 4*(2));
}

