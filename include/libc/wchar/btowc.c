#include <wchar.h>
#include <stdlib.h>

wint_t btowc(int c)
{
  unsigned char ch = c;

  if (MB_CUR_MAX == 1) {
    return ch;
  } else {
    if (ch < 128) return ch;
  }
  return WEOF;
}
