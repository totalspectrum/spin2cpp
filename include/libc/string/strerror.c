#include <errno.h>
#include <string.h>

static char *_sys_errlist[] = {
  "OK",
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
  "Bad address",
  "Broken connection",
  "Device or resource busy",
  "Cross device link",
  "No space on device",
  "Unknown error",
};

#define ENUMERRORS (sizeof(_sys_errlist)/sizeof(_sys_errlist[0]))

char *_strerror(int errnum)
{
    if (errnum < 0 || errnum >= (int)ENUMERRORS)
        errnum = ENUMERRORS-1;
    return _sys_errlist[errnum];
}

char *strerror(int errnum)
{
    return _strerror(errnum);
}
