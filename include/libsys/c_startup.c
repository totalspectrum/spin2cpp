//
// startup code for C
//
#include <stdlib.h>

static const char *_argv[] = {
    "program",
    NULL
};

#define _argc (sizeof(_argv) / sizeof(_argv[0]))

void _c_startup()
{
    int r = main(_argc, _argv);
    _Exit(r);
}
