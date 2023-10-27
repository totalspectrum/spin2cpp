#define __SPIN2CPP__
#include <propeller.h>
#include "test064.h"

unsigned char test064::dat[] = {
  0x00, 0x00, 0x10, 0x41, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 
};
int32_t *test064::Getit(void)
{
  return (((int32_t *)&dat[0]));
}

