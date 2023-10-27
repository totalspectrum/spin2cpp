#define __SPIN2CPP__
#include <propeller.h>
#include "test106.h"

unsigned char volatile test106::dat[] = {
  0x00, 0x00, 0x00, 0x00, 
};
int32_t test106::Foo(void)
{
  return (((int32_t *)&dat[0])[0] + 1);
}

