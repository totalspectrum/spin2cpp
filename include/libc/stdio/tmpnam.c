#include <stdio.h>
#include <unistd.h>
#include <string.h>

char *
tmpnam(char *buf)
{
  static char tbuf[L_tmpnam];
  if (!buf) {
    buf = tbuf;
  }
  strcpy(buf, "/tmp/tpXXXXXX");
  return _mktemp(buf);
}
