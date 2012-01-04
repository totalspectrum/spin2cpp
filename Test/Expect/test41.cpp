#include <propeller.h>
#include "test41.h"

int32_t test41::rx(void)
{
  int32_t rxbyte = 0;
  while (((rxbyte = _OUTA) < 0)) {
  }
  return rxbyte;
}

