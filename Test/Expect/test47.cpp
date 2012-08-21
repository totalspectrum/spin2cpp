#include <propeller.h>
#include "test47.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test47::Test(int32_t C)
{
  int32_t result = 0;
  if (Flag == 0) {
    if (C == 9) {
      do {
        OUTA = (Cols++);
      } while (Cols & 0x7);
    } else if (C == 13) {
      OUTA = C;
    } else if (1) {
      OUTA = Flag;
    }
  } else if (Flag == 10) {
    Cols = (C % Cols);
  }
  Flag = 0;
  return result;
}

