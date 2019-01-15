#include <ctype.h>

#undef ispunct

int ispunct(int c)
{
  return __isctype(c, _CTp);
}
