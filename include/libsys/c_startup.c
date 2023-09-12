//
// startup code for C
//
#include <sys/ioctl.h>
#include <sys/argv.h>
#include <stdlib.h>

void _c_startup()
{
    int r;
    int argc = 0;
    char **argv;
    
#ifdef __FLEXC_NO_CRNL__
    _setrxtxflags(TTY_FLAG_ECHO); // make sure TTY_FLAG_CRNL is off
#endif
    
    _waitms(20);  // brief pause
    argv = __fetch_argv(&argc);
    r = main(argc, argv);
    _waitms(20);  // brief pause
    _Exit(r);
}
