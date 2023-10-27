#define __SPIN2CPP__
#include <propeller.h>
#include "test056.h"

unsigned char test056::dat[] = {
  0x00, 0x00, 0x80, 0x3f, 
};
int32_t test056::Getval(void)
{
  return ((int32_t *)&dat[0])[0];
}

