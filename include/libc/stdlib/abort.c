#include <stdlib.h>

void
abort(void)
{
  _Exit(0xff);
}
