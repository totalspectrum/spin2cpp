#include <stdio.h>
#include <errno.h>
#include <string.h>

void perror(const char *s)
{
  int err = _geterror();

  fputs(s, stderr);
  fputs(": ", stderr);
  fputs(strerror(err), stderr);
  fputs("\n", stderr);
}
