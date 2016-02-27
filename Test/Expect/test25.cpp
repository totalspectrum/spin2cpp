#include <propeller.h>
#include "test25.h"

#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
int32_t test25::Unlock(void)
{
  if (X != 0) {
    return PostEffect__(X, 0);
  }
  return 0;
}

