#define __SPIN2CPP__
#include <propeller.h>
#include "test067.h"

unsigned char test067::dat[] = {
  0x3b, 0xaa, 0xb8, 0x3f, 
};
int32_t test067::Getx(void)
{
  return ((int32_t *)&dat[0])[0];
}

