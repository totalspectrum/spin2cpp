#include <wctype.h>

int
iswprint(wint_t wc)
{
  return iswctype(wc, _CTprint);
}
