#define __SPIN2CPP__
#include <propeller.h>
#include "test041.h"

#define INLINE__ static inline
INLINE__ int32_t Lookup__(int32_t x, int32_t b, int32_t a[], int32_t n) { int32_t i = (x)-(b); return ((unsigned)i >= n) ? 0 : (a)[i]; }

int32_t test041::Hexdigit(int32_t X)
{
  static int32_t look__0001[] = {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, };

  return Lookup__((X & 0xf), 0, look__0001, 16);
}

