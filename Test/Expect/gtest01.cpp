#include <propeller.h>
#include "gtest01.h"

#line 4 "gtest01.spin"
int32_t gtest01::foo(void)
{
#line 5 "gtest01.spin"
  if (x < 2) {
    return 0;
  } else {
    return 1;
  }
}

