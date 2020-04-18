#include <stdio.h>
#include <errno.h>
#include <string.h>

char *_sys_errlist[] = {
  "Unknown error",
  "Numerical argument out of domain",
  "Result not representable",
  "Illegal multibyte sequence",
  "No such file or directory",
  "Bad file number",
  "Permission denied",
  "Not enough memory",
  "Temporary failure",
  "File exists",
  "Invalid argument",
  "Too many open files",
  "I/O error",
  "Not a directory",
  "Is a directory",
  "Read only file system",
  "Function not implemented",
  "Directory not empty",
  "Name too long",
  "Device not seekable",
};

#define ENUMERRORS (sizeof(_sys_errlist)/sizeof(_sys_errlist[0]))

char *strerror(int errnum)
{
    if (errnum < 0 || errnum >= (int)ENUMERRORS)
        errnum = 0;
    return _sys_errlist[errnum];
}

void perror(const char *s)
{
  int err = _geterror();

  fputs(s, stderr);
  fputs(": ", stderr);
  fputs(strerror(err), stderr);
  fputs("\n", stderr);
}
