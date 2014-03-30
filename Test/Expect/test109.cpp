#include <propeller.h>
#include "test109.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test109::Readdelta(int32_t Encid)
{
  int32_t Deltapos = 0;
  Deltapos = 0 + (Encid < Totdelta);
  return Deltapos;
}

