/* original from the public domain libnix library */

#include <wchar.h>

int
wcscoll (const wchar_t *scan1, const wchar_t *scan2)
{
  return wcscmp (scan1, scan2);
}
