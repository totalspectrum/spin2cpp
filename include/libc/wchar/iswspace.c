#include <wctype.h>

int
iswspace(wint_t wc)
{
  return iswctype(wc, _CTs);
}
