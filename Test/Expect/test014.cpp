#define __SPIN2CPP__
#include <propeller.h>
#include "test014.h"

unsigned char test014::dat[] = {
  0x01, 0x00, 0x00, 0x00, 0xdd, 0xcc, 0xbb, 0xaa, 
};
int32_t test014::Myfunc(void)
{
  return ((int32_t *)&dat[4])[0];
}

