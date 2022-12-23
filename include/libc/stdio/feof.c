#include <stdio.h>
#undef feof
#undef ferror

int feof(FILE *f) {
    return (0 != ((f)->state & _VFS_STATE_EOF));
}

int ferror(FILE *f) {
    return (0 != ((f)->state & _VFS_STATE_ERR));
}
