#include <ctype.h>

#undef iscntrl

int iscntrl(int c)
{
  return __isctype(c, _CTc);
}
