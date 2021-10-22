#define __SPIN2CPP__
#include <propeller.h>
#include "test103.h"

int32_t test103::Blah(int32_t X, int32_t Y)
{
  return -(X >= Y);
}

int32_t test103::Foo(int32_t X, int32_t Y)
{
  return -(X <= Y);
}

int32_t test103::Blah2(int32_t X)
{
  J = -(J >= X);
  return J;
}

