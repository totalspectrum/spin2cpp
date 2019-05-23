/* 
  Counter object
 */
#include <propeller.h>
#include "ctest01.h"

#if defined(__GNUC__)
#define INLINE__ static inline
#else
#define INLINE__ static
#ifndef __FLEXC__
#define waitcnt(n) _waitcnt(n)
#define coginit(id, code, par) _coginit((unsigned)(par)>>2, (unsigned)(code)>>2, id)
#define cognew(code, par) coginit(0x8, (code), (par))
#define cogstop(i) _cogstop(i)
#endif /* __FLEXC__ */
#endif

void ctest01_add(ctest01 *self, int32_t x)
{
  self->cntr = self->cntr + x;
}

void ctest01_inc(ctest01 *self)
{
  ctest01_add(self, 1);
}

void ctest01_dec(ctest01 *self)
{
  (--self->cntr);
}

int32_t ctest01_get(ctest01 *self)
{
  return self->cntr;
}

int32_t ctest01_double(int32_t x)
{
  return (x * x);
}

