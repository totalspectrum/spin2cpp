#include <propeller.h>
#include "test85.h"

void test85::Main(void)
{
  Fds.Start(31, 30, 0, 115200);
  Fds.Str((char *)"object array test\r\n");
  Printn(0);
  Printn(1);
  Printn(2);
  Fds.Str((char *)"increment v[0]\n\r");
  // should be the same as v[0].incn
  V[0].Incn();
  Printn(0);
  Printn(1);
  Printn(2);
}

void test85::Printn(int32_t I)
{
  int32_t 	R;
  Fds.Str((char *)"v[");
  Fds.Dec(I);
  Fds.Str((char *)"] = ");
  R = V[I].Getn();
  Fds.Dec(R);
  Fds.Str((char *)"\n\r");
}

