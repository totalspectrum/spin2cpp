#define __SPIN2CPP__
#include <propeller.h>
#include "test011.h"

int32_t test011::Peek(void)
{
  return Count;
}

int32_t test011::Donext(void)
{
  Count = Peek() + 1;
  return Count;
}

