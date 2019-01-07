#include <stdlib.h>
#include <wchar.h>
#include <stdio.h>

int wctob(wint_t c)
{
  if (MB_CUR_MAX == 1) {
    if (256 > (unsigned)c)
      return c;
  } else {
    if (128 > (unsigned)c)
      return c;
  }
  return EOF;
}
