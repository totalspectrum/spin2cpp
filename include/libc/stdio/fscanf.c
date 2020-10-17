#include <stdio.h>
#include <stdarg.h>

int fscanf(FILE *f, const char *format,...)
{ int retval;
  va_list args;
  va_start(args,format);
  retval=vfscanf(f, format,args);
  va_end(args);
  return retval;
}
