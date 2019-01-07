#include <wctype.h>

int
iswpunct(wint_t wc)
{
  return iswctype(wc, _CTp);
}
