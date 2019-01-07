#include <wctype.h>

int
iswupper(wint_t wc)
{
  return iswctype(wc, _CTu);
}
