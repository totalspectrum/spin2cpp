#include <wctype.h>

int
iswcntrl(wint_t wc)
{
  return iswctype(wc, _CTc);
}
