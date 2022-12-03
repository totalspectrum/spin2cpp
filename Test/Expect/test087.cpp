#define __SPIN2CPP__
#include <propeller.h>
#include "test087.h"

int32_t test087::Check(int32_t N)
{
  int32_t Sum;
  Sum = 0;
  if (N > 1) {
    while (N--) {
      Sum = Sum + N;
    }
  } else {
    /*  other part  */
    if (N < (-1)) {
      return (-1);
    } else {
      return 0;
    }
  }
  return Sum;
}

