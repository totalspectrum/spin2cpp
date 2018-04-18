#include <propeller.h>
#include "gtest02.h"

#line 1 "gtest02.spin"
void gtest02::run(void)
{
  int32_t 	i, now, elapsed, x;
#line 8 "gtest02.spin"
  for(i = 1; i < 16; i++) {
#line 4 "gtest02.spin"
    now = CNT;
    x = fibo(i);
    elapsed = CNT - now;
  }
}

#line 8 "gtest02.spin"
int32_t gtest02::fibo(int32_t x)
{
#line 9 "gtest02.spin"
  if (x < 2) {
    return x;
  }
#line 11 "gtest02.spin"
  return (fibo(x - 1) + fibo(x - 2));
}

