#include <propeller.h>
#include "test66.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test66::Myinit(int32_t A, int32_t B)
{
  int32_t _parm__0000[] = { 0, A, B };
  _parm__0000[1] = 1;
  memmove( (void *)&_parm__0000[0], (void *)&Ga, 4*(3));
  return _parm__0000[0];
}

