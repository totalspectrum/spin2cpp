#define __SPIN2CPP__
#include <propeller.h>
#include "test146.h"

void test146::Swapab(void)
{
  { Tuple2__ tmp__ = MakeTuple2__(B, A); A = tmp__.v0; B = tmp__.v1;  };
}

Tuple3__ test146::Seq3(int32_t N)
{
  return MakeTuple3__(N, N + 1, N + 2);
}

void test146::Setit(void)
{
  { Tuple3__ tmp__ = Seq3(1); A = tmp__.v0; B = tmp__.v1; C = tmp__.v2;  };
}

