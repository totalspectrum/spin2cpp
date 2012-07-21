#include <propeller.h>
#include "test41.h"

#define Lookup__(x, b, a, n) __extension__({ int32_t i = (x)-(b); ((unsigned)i >= n) ? 0 : (a)[i]; })

int32_t test41::Hexdigit(int32_t X)
{
  int32_t result = 0;
  return Lookup__((X & 0xf), 0, ((int32_t[]){48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, }), 16);
}

