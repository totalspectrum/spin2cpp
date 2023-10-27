#define __SPIN2CPP__
#include <propeller.h>
#include "test057.h"

unsigned char test057::dat[] = {
  0x00, 0x00, 0x80, 0x40, 
};
int32_t test057::Getval(void)
{
  return ((int32_t *)&dat[0])[0];
}

