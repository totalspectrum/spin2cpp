#include <propeller.h>
#include "test144.h"

void test144::Func1(void)
{
  ((char *)(((int32_t)Func2()) + 11))[0] |= 0x20;
}

int32_t *test144::Func2(void)
{
  return (Buf);
}

