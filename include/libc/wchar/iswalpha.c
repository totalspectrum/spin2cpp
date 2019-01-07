#include <wctype.h>

int
iswalpha(wint_t wc)
{
  return iswctype(wc, _CTalpha);
}
