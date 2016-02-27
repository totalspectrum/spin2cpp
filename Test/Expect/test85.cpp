#include <propeller.h>
#include "test85.h"

void test85::Main(void)
{
  Fds.Start(31, 30, 0, 115200);
  Fds.Str((int32_t)"object array test\r\n");
  Printn(0);
  Printn(1);
  Printn(2);
  Fds.Str((int32_t)"increment v[0]\n\r");
  // should be the same as v[0].incn
  V[0].Incn();
  Printn(0);
  Printn(1);
  Printn(2);
}

void test85::Printn(int32_t I)
{
  int32_t	R;
  Fds.Str((int32_t)"v[");
  Fds.Dec(I);
  Fds.Str((int32_t)"] = ");
  R = V[I].Getn();
  Fds.Dec(R);
  Fds.Str((int32_t)"\n\r");
}

