// 
// CON sample
// 
// CON A = 1, B = 2
#include <propeller.h>
#include "test100.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test100::Foo(void)
{
  return Something;
}

int32_t test100::Foo2(void)
{
  return C;
}

