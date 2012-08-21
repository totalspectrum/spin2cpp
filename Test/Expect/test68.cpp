#include <propeller.h>
#include "test68.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test68::Foo(int32_t N)
{
  int32_t result = 0;
  {
    int32_t _limit__0000 = N;
    int32_t _step__0001 = 1;
    result = 1;
    if (result >= _limit__0000) _step__0001 = -_step__0001;
    do {
      _OUTA = result;
      result = (result + _step__0001);
    } while (((_step__0001 > 0) && (result <= _limit__0000)) || ((_step__0001 < 0) && (result >= _limit__0000)));
  }
  return result;
}

