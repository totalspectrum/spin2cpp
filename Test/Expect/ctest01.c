/* 
  Counter object
 */
#include <propeller.h>
#include "ctest01.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#define waitcnt(n) _waitcnt(n)
#define coginit(id, code, par) _coginit((unsigned)(par)>>2, (unsigned)(code)>>2, id)
#define cognew(code, par) coginit(0x8, (code), (par))
#define cogstop(i) _cogstop(i)
#endif

void ctest01_add(ctest01 *self, int32_t x)
{
  self->Cntr = self->Cntr + x;
}

void ctest01_inc(ctest01 *self)
{
  ctest01_add(self, 1);
}

void ctest01_dec(ctest01 *self)
{
  (--self->Cntr);
}

int32_t ctest01_Get(ctest01 *self)
{
  return self->Cntr;
}

int32_t ctest01_Double(int32_t x)
{
  return (x * x);
}

