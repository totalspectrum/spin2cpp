#define __SPIN2CPP__
#include <propeller.h>
#include "test139.h"

unsigned char test139::dat[] = {
  0x00, 0x00, 0x00, 0x00, 
};
void test139::Setval(int32_t X)
{
  (((int32_t *)&dat[0]))[0] = X;
}

