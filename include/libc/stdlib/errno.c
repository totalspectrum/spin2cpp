#include <errno.h>
#undef errno

static int errno;

int _geterror() { int r = errno; errno = 0; return r; }
int _seterror(int num) { errno = num; if (num) { return -1; } else return 0; }

int *_geterrnoptr() { return &errno; }
