#include <propeller.h>
#include "test085.h"

void test085::Main(void)
{
  Fds.Start(31, 30, 0, 115200);
  Fds.Str("object array test\r\n");
  Printn(0);
  Printn(1);
  Printn(2);
  Fds.Str("increment v[0]\n\r");
  // should be the same as v[0].incn
  V[0].Incn();
  Printn(0);
  Printn(1);
  Printn(2);
}

void test085::Printn(int32_t I)
{
  int32_t 	R;
  Fds.Str("v[");
  Fds.Dec(I);
  Fds.Str("] = ");
  R = V[I].Getn();
  Fds.Dec(R);
  Fds.Str("\n\r");
}

