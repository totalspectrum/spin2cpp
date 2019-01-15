#include <ctype.h>

#undef isgraph

int isgraph(int c)
{
  return (!__isctype(c, (_CTc|_CTs)) && (__ctype_get(c) != 0));
}
