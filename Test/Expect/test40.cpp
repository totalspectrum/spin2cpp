#include <stdlib.h>
#include <propeller.h>
#include "test40.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test40::Tx(int32_t Character)
{
  OUTA = Character;
  return 0;
}

int32_t test40::Dec(int32_t Value)
{
  int32_t	I, X;
  int32_t result = 0;
  X = -(Value == (int32_t)0x80000000U);
  if (Value < 0) {
    Value = (abs((Value + X)));
    Tx('-');
  }
  I = 1000000000;
  {
    int32_t _idx__0000;
    for(_idx__0000 = 1; _idx__0000 <= 10; (_idx__0000 = (_idx__0000 + 1))) {
      if (Value >= I) {
        Tx((((Value / I) + '0') + (X * -(I == 1))));
        Value = (Value % I);
        result = -1;
      } else {
        if ((result) || (I == 1)) {
          Tx('0');
        }
      }
      I = (I / 10);
    }
  }
  return result;
}

