#include <propeller.h>
#include "test24.h"

#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
int32_t test24::Unlock(void)
{
  return PostEffect__(X, 0);
}

