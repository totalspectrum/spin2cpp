#include <propeller.h>
#include "test60.h"

int32_t test60::func(int32_t a, int32_t b)
{
  int32_t ok = 0;
  if (a < b) {
    ok = -1;
  } else {
    if (a == 0) {
      ok = -1;
    } else {
      ok = 0;
    }
  }
  return ok;
}

