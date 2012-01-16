#include <propeller.h>
#include "test38.h"

extern inline int32_t Min__(int32_t a, int32_t b) { return a < b ? a : b; }
extern inline int32_t Max__(int32_t a, int32_t b) { return a > b ? a : b; }
int32_t test38::big(int32_t x, int32_t y)
{
  int32_t result = 0;
  result = (Max__(x, y));
  return result;
}

int32_t test38::small(int32_t x, int32_t y)
{
  int32_t result = 0;
  result = (Min__(x, y));
  return result;
}

