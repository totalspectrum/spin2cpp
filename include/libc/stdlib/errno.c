#include <errno.h>
#undef errno

int errno;

int _geterror() { return errno; }
int _seterror(int num) { errno = num; return -1; }
