/* simple fopen implementation */
/* written by Eric R. Smith, Total Spectrum Software Inc.
 * and dedicated to the public domain
 */

//#define DEBUG

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

FILE *_fopenraw(const char *pathname, const char *mode, vfs_file_t *ftab) _COMPLEXIO
{
    int want_read = 0;
    int want_write = 0;
    int want_append = 0;
    int want_binary = 0;
    int want_create = 0;
    int want_trunc = 0;
    unsigned int open_mode;
    int c, r;
    
    while ((c = *mode++) != 0) {
        switch (c) {
        case 'r':
            want_read = 1;
            break;
        case 'w':
            want_create = 1;
            want_trunc = 1;
            want_write = 1;
            break;
        case 'a':
            want_write = 1;
            want_create = 1;
            want_append = 1;
            break;
        case 'b':
            want_binary = 1;
            break;
        case '+':
            if (want_read) {
                want_write = 1;
            } else if (want_write) {
                want_read = 1;
                break;
            } else {
                _seterror(EINVAL);
                return 0;
            }
            break;
        default:
            _seterror(EINVAL);
            return 0;
        }
    }
    if (want_read) {
        if (want_write) {
            open_mode = O_RDWR;
        } else {
            open_mode = O_RDONLY;
        }
    } else if (want_write) {
        open_mode = O_WRONLY;
    } else {
        _seterror(EINVAL);
        return 0;
    }
    if (want_append) open_mode |= O_APPEND;
    if (want_create) open_mode |= O_CREAT;
    if (want_trunc) open_mode |= O_TRUNC;

    r = _openraw(ftab, pathname, open_mode, 0666);
    if (r != 0) {
#ifdef DEBUG
        __builtin_printf("fopen returning NULL\n");
#endif        
        return 0;
    }
    if (_isatty(ftab)) {
        ftab->state |= _VFS_STATE_ISATTY;
    }
#ifdef DEBUG
    __builtin_printf("fopen returning %x state=%x\n", ftab, ftab->state);
#endif        
    return ftab;
}

int fclose(FILE *f) _COMPLEXIO
{
#ifdef _DEBUG
    __builtin_printf("fclose(%x)\n", (unsigned)f);
#endif    
    return _closeraw(f);
}

FILE *fopen(const char *pathname, const char *mode)
{
    vfs_file_t *ftab;
    int fd = _find_free_file();
    if (fd < 0) return 0;

    ftab = __getftab(fd);
    return _fopenraw(pathname, mode, ftab);
    
}

FILE *freopen(const char *pathname, const char *mode, FILE *stream)
{
    if (!pathname) {
        // C99 says it may be possible to change the mode of an existing stream
        // but we don't support this
        return NULL;
    }
    return _fopenraw(pathname, mode, stream);
}
