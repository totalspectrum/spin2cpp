#define __SPIN2CPP__
#include <propeller.h>
#include "test054.h"

unsigned char test054::dat[] = {
  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};
int32_t *test054::Getfoo(void)
{
  return (((int32_t *)&dat[4]));
}

