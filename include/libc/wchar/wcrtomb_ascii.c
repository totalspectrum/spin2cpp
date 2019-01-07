#include <wchar.h>
#include <stdint.h>
#include <errno.h>

size_t
_wcrtomb_ascii(char *s, wchar_t wcorig, mbstate_t *ps)
{
  uint32_t wc = wcorig;

  if (wc <= 0xff) {
    *s++ = wc;
    return 1;
  } else {
    errno = EILSEQ;
    return (size_t)-1;
  }
}

size_t (*_wcrtomb_ptr)(char *, wchar_t, mbstate_t *) = _wcrtomb_ascii;

size_t
wcrtomb( char *s, wchar_t wc, mbstate_t *MB)
{
    return (*_wcrtomb_ptr)(s, wc, MB);
}
