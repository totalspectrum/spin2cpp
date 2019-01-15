#include <ctype.h>

#undef isxdigit

int isxdigit(int c)
{
  return __isctype(c, _CTx);
}
