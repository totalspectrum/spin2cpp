#include <stdio.h>
#ifndef _MAX_FILES
#define _MAX_FILES 8
#endif

static int _nullput(int c) {
    return -1;
}

static int _nullget() {
    return -1;
}

static int _nullclose() {
    return 0;
}

static int _serget() {
    return _rx();
}
static int _serput(int c) {
    _tx(c);
    return 1;
}

FILE _stdin = { &_nullput, &_serget, &_nullclose };
FILE _stdout = { &_serput, &_nullget, &_nullclose };
FILE _stderr = { &_serput, &_nullget, &_nullclose };

FILE *__ftab[_MAX_FILES] = {
    &_stdin,
    &_stdout,
    &_stderr
};

FILE *__getftab(int i) {
    if ((unsigned)i >= _MAX_FILES) {
        return NULL;
    }
    return __ftab[i];
}
