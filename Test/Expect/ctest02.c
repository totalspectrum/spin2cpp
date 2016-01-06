/* 
  Check for private method declarations
 */
#include <propeller.h>
#include "ctest02.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#define waitcnt(n) _waitcnt(n)
#define coginit(id, code, par) _coginit((unsigned)(par)>>2, (unsigned)(code)>>2, id)
#define cognew(code, par) coginit(0x8, (code), (par))
#define cogstop(i) _cogstop(i)
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

