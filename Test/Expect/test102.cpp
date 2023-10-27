#define __SPIN2CPP__
#include <propeller.h>
#include "test102.h"

unsigned char test102::dat[] = {
  0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 
};
int32_t test102::Foo(void)
{
  return ((int32_t *)&dat[4])[0];
}

