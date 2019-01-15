#include <ctype.h>

#undef isprint

int isprint(int c)
{
  return (!__isctype(c, (_CTc)) && (__ctype_get(c) != 0));
}
