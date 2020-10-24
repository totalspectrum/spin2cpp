/* simple fseek implementation */
/* written by Eric R. Smith, Total Spectrum Software Inc.
 * and dedicated to the public domain
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>

int fseek(FILE *f, long offset, int whence)
{
    int fd = fileno(f);
    int r;

    fflush(f);
    r = lseek(fd, offset, whence);
    if (r != -1) {
        r = 0;  // fseek returns 0 on success
    }
    return r;
}

long ftell(FILE *f)
{
    int fd = fileno(f);
    long r;

    fflush(f);
    r = lseek(fd, 0, SEEK_CUR);
    return r;
}
