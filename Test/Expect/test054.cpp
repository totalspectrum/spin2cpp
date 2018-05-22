#include <propeller.h>
#include "test054.h"

char test054::dat[] = {
  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};
int32_t *test054::Getfoo(void)
{
  return (((int32_t *)&dat[4]));
}

