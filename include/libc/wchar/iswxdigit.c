#include <wctype.h>

int
iswxdigit(wint_t wc)
{
  return iswctype(wc, _CTx);
}
