#include <wctype.h>

int
iswblank(wint_t wc)
{
  return iswctype(wc, _CTb);
}
