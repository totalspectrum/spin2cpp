#include <propeller.h>
#include "test86.h"

void test86::Set1(void)
{
  OUTA = (OUTA & 0xfffffffd) | 0x2;
}

void test86::Set(int32_t Pin)
{
  OUTA = (OUTA & (~(1 << Pin))) | (1 << Pin);
}

