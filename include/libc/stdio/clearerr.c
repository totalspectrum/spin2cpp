#include <stdio.h>

void clearerr(FILE *f)
{
    f->state &= ~(_VFS_STATE_ERR|_VFS_STATE_EOF);
}
