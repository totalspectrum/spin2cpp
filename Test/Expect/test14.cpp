#include <propeller.h>
#include "test14.h"

char test14::dat[] = {
  0x01, 0x00, 0x00, 0x00, 0xdd, 0xcc, 0xbb, 0xaa, 
};
int32_t test14::Myfunc(void)
{
  return ((int32_t *)&dat[4])[0];
}

