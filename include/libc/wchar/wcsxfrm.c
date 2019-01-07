/* original from the public domain libnix library */

#include <wchar.h>

size_t
wcsxfrm (wchar_t *to, const wchar_t *from, size_t maxsize)
{
  wcsncpy (to, from, maxsize);
  return wcslen (from);
}
