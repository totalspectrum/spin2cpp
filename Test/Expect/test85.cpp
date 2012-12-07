#include <propeller.h>
#include "test85.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test85::Main(void)
{
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
  return 0;
}

int32_t test85::Printn(int32_t I)
{
  int32_t	R;
  Fds.Str((int32_t)"v[");
  Fds.Dec(I);
  Fds.Str((int32_t)"] = ");
  R = V[I].Getn();
  Fds.Dec(R);
  Fds.Str((int32_t)"\n\r");
  return 0;
}

