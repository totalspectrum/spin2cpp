#include <wchar.h>

/*
 * determine whether ps is NULL or is an initial state
 */
int
mbsinit(const mbstate_t *ps)
{
  if (!ps || ps->left == 0)
    return 1;
  return 0;
}
