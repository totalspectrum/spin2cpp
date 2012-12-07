#include <propeller.h>
#include "test11.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test11::Peek(void)
{
  return Count;
}

int32_t test11::Donext(void)
{
  Count = (Peek() + 1);
  return Count;
}

