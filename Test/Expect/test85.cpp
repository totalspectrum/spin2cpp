#include <propeller.h>
#include "test85.h"

int32_t test85::Main(void)
{
  int32_t result = 0;
  Fds.Start(31, 30, 0, 115200);
  Fds.Str((int32_t)"object array test\r\n");
  Printn(0);
  Printn(1);
  Printn(2);
  Fds.Str((int32_t)"increment v[0]\n\r");
  V[0].Incn();
  Printn(0);
  Printn(1);
  Printn(2);
  return result;
}

int32_t test85::Printn(int32_t I)
{
  int32_t result = 0;
  int32_t	R;
  Fds.Str((int32_t)"v[");
  Fds.Dec(I);
  Fds.Str((int32_t)"] = ");
  R = V[I].Getn();
  Fds.Dec(R);
  Fds.Str((int32_t)"\n\r");
  return result;
}

