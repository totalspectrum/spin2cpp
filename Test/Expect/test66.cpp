#include <propeller.h>
#include "test66.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test66::Myinit(int32_t A, int32_t B)
{
  int32_t _parm__0000[3];
  _parm__0000[0] = 0;
  _parm__0000[1] = A;
  _parm__0000[2] = B;
  _parm__0000[1] = 1;
  memmove( (void *)&_parm__0000[0], (void *)&Ga, 4*(3));
  return _parm__0000[0];
}

