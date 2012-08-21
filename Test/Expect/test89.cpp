#include <propeller.h>
#include "test89.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test89::Catchlong(int32_t Longvar)
{
  int32_t _parm__0000[] = { Longvar };
  int32_t result = 0;
  B0 = ((uint8_t *)(int32_t)(&_parm__0000[0]))[0];
  B1 = ((uint8_t *)(int32_t)(&_parm__0000[0]))[1];
  B2 = ((uint8_t *)(int32_t)(&_parm__0000[0]))[2];
  B3 = ((uint8_t *)(int32_t)(&_parm__0000[0]))[3];
  return result;
}

