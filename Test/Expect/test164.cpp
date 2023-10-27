// Does not compile
#define __SPIN2CPP__
#include <propeller.h>
#include "test164.h"

unsigned char test164::dat[] = {
  0x01, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00, 
};
void test164::Main(void)
{
  int32_t 	Tmp1, Tmp2;
  Tmp1 = 0x4;
  _OUTA = Tmp1;
  Tmp2 = ((int32_t)(((int32_t *)&dat[4]))) - 2;
  _OUTB = Tmp2;
}

