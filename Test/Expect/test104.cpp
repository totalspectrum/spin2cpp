#define __SPIN2CPP__
#include <propeller.h>
#include "test104.h"

unsigned char test104::dat[] = {
  0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x81, 0x01, 0x00, 0x00, 
};
int32_t *test104::Get(void)
{
  return (((int32_t *)&dat[0]));
}

