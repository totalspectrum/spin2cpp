/* simple towlower/towupper, valid only for the C locale */
#include <wctype.h>

wint_t
towlower(wint_t x)
{
  if (x >= L'A' && x <= L'Z')
    x = (x - L'A') + L'a';
  return x;
}

wint_t
towupper(wint_t x)
{
  if (x >= L'a' && x <= L'z')
    x = (x - L'a') + L'A';
  return x;
}
