#include <propeller.h>
#include "test29.h"

int32_t test29::tx(int32_t val)
{
  int32_t result = 0;
  ((uint8_t *)28672)[idx] = 0;
  return result;
}

int32_t test29::str(int32_t stringptr)
{
  int32_t result = 0;
  while (lockset(strlock)) {
  }
  for (int _tmp__0000 = strlen(stringptr); _tmp__0000 > 0; --_tmp__0000) {
    tx(((uint8_t *)(stringptr++))[0]);
  }
  lockclr(strlock);
  return result;
}

