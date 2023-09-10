//
// startup code for C
//
#include <sys/ioctl.h>
#include <stdlib.h>

#define MAX_ARGC 32
#define ARGV_MAGIC ('A' | ('R' << 8) | ('G'<<16) | ('v'<<24))

#define ARGV_ADDR  0xFC000
#define START_ARGS ((char *)(ARGV_ADDR+4))
#define END_ARGS   ((char *)(ARGV_ADDR+0xFFF))

static const char *_argv[MAX_ARGC];

void _c_startup()
{
    int r;
    int argc = 0;

#ifdef __FLEXC_NO_CRNL__
    _setrxtxflags(TTY_FLAG_ECHO); // make sure TTY_FLAG_CRNL is off
#endif
    
    _waitms(20);  // brief pause
    _argv[argc++] = __FLEXSPIN_PROGRAM__;
#ifdef __P2__    
    if ( ARGV_MAGIC == (*(long *)ARGV_ADDR) ) {
        char *arg = START_ARGS;
        while (arg < END_ARGS && *arg != 0 && argc < MAX_ARGC-1) {
            _argv[argc++] = arg;
            while (*arg != 0 && arg < END_ARGS) arg++;
            arg++;
        }
    }
#endif    
    _argv[argc] = NULL;
    r = main(argc, _argv);
    _waitms(20);  // brief pause
    _Exit(r);
}
