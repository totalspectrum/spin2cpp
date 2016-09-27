#include <propeller.h>
#include "test136.h"

void test136::Doone(int32_t X)
{
  switch(X) {
  case 0:
    Dotwo(1);
    break;
  case 1:
    Dotwo(2);
    break;
  default:
    Dotwo(3);
    break;
  }
}

void test136::Dotwo(int32_t X)
{
  OUTA = X;
}

