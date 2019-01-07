#include <wctype.h>

int
iswdigit(wint_t wc)
{
  return iswctype(wc, _CTd);
}
