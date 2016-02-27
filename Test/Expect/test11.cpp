#include <propeller.h>
#include "test11.h"

int32_t test11::Peek(void)
{
  return Count;
}

int32_t test11::Donext(void)
{
  Count = Peek() + 1;
  return Count;
}

