/* 
  Check for private method declarations
 */
#define __SPIN2CPP__
#include <propeller.h>
#include "ctest02.h"

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
#ifdef __CATALINA__
#define _CNT CNT
#define _clkfreq _clockfreq()
#endif
#endif

static  int32_t ctest02_implement(ctest02 *self, int32_t x);

int32_t ctest02_myfunc(ctest02 *self, int32_t x)
{
  return ctest02_implement(self, x);
}

static int32_t ctest02_implement(ctest02 *self, int32_t x)
{
  return (self->val * x);
}

