#define __SPIN2CPP__
#include <propeller.h>
#include "test161.h"

unsigned char test161::dat[] = {
  0x20, 0x40, 0x41, 0x20, 
};
void test161::Demo(void)
{
  _OUTA = ((char *)&dat[0])[0];
}

