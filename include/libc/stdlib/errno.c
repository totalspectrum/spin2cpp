#include <errno.h>
#undef errno

int errno;

int _geterror() { return errno; }
int _seterror(int num) { if (num) { errno = num; return -1; } else return 0; }
