#include <propeller.h>
#include "test50.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test50::Myinit(int32_t A, int32_t B, int32_t C)
{
  int32_t _parm__0000[3];
  int32_t result = 0;
  _parm__0000[0] = A;
  _parm__0000[1] = B;
  _parm__0000[2] = C;
  memmove( (void *)&Gb, (void *)&_parm__0000[1], 4*(2));
  return result;
}

