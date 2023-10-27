#define __SPIN2CPP__
#include <propeller.h>
#include "test058.h"

unsigned char test058::dat[] = {
  0xdb, 0x0f, 0xc9, 0x3f, 
};
int32_t test058::Getval(void)
{
  return ((int32_t *)&dat[0])[0];
}

