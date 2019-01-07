#include <wctype.h>

int
iswlower(wint_t wc)
{
  return iswctype(wc, _CTl);
}
