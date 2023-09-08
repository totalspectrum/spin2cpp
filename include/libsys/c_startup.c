//
// startup code for C
//
#include <sys/ioctl.h>
#include <stdlib.h>

#define MAX_ARGC 32
#define ARGV_MAGIC ('A' | ('R' << 8) | ('G'<<16) | ('v'<<24))

#define START_ARGS ((char *)0xFC004)
#define END_ARGS   ((char *)0xFCFFF)

static const char *_argv[MAX_ARGC];

void _c_startup()
{
    int r;
    int argc = 0;

    _setrxtxflags(TTY_FLAG_ECHO); // make sure TTY_FLAG_CRNL is off
    _waitms(20);  // brief pause

    _argv[argc++] = "program";
    if ( ARGV_MAGIC == (*(long *)0xFC000) ) {
        char *arg = START_ARGS;
        while (arg < END_ARGS && *arg != 0 && argc < MAX_ARGC-1) {
            _argv[argc++] = arg;
            while (*arg != 0 && arg < END_ARGS) arg++;
            arg++;
        }
    }
    _argv[argc] = NULL;
    r = main(argc, _argv);
    _waitms(20);  // brief pause
    _Exit(r);
}
