#include <propeller.h>
#include "test38.h"

#define Min__(x, y) __extension__({ int32_t a = (x); int32_t b = (y); a < b ? a : b;})
#define Max__(x, y) __extension__({ int32_t a = (x); int32_t b = (y); a > b ? a : b;})

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

