#include <sys/argv.h>
#ifndef NULL
#define NULL 0
#endif

static const char *_argv[MAX_ARGC];
static int _argc = -1;

char **__fetch_argv(int *argc_p) {
    int argc = 0;
    if (_argc < 0) {
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
        _argc = argc;
    }
    if (argc_p) {
        *argc_p = _argc;
    }
    return _argv;
}

/*
 * fetch command argument i
 */
const char *
_command_str(int i)
{
    char **argv = __fetch_argv(NULL);
    if (i < 0 || i >= _argc) return NULL;
    return argv[i];
}
