#include <wctype.h>

int
iswgraph(wint_t wc)
{
  return iswctype(wc, _CTgraph);
}
