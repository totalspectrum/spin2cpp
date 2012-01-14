#include <propeller.h>
#include "test11.h"

int32_t test11::peek(void)
{
  int32_t result = 0;
  return count;
}

int32_t test11::donext(void)
{
  int32_t result = 0;
  count = (peek() + 1);
  return count;
}

