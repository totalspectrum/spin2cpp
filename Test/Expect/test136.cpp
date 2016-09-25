#include <propeller.h>
#include "test136.h"

void test136::Doone(int32_t X)
{
  if (X == 0) {
    Dotwo(1);
  } else if (X == 1) {
    Dotwo(2);
  } else if (1) {
    Dotwo(3);
  }
}

void test136::Dotwo(int32_t X)
{
  OUTA = X;
}

