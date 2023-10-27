#define __SPIN2CPP__
#include <propeller.h>
#include "test018.h"

unsigned char test018::dat[] = {
  0xf1, 0x05, 0xbc, 0xa0, 0x01, 0x00, 0x7c, 0x5c, 0x01, 0x00, 0x00, 0x00, 
};
int32_t *test018::Start(void)
{
  return (((int32_t *)&dat[0]));
}

