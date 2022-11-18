#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define S_IRUSR   0400
#define S_IWUSR   0200
#define S_IXUSR   0100
#define S_IRGRP   0040
#define S_IWGRP   0020
#define S_IXGRP   0010
#define S_IROTH   0004
#define S_IWOTH   0002
#define S_IXOTH   0001

#define S_IRWXU  (S_IRUSR|S_IWUSR|S_IXUSR)
#define S_IRWXG  (S_IRGRP|S_IWGRP|S_IXGRP)
#define S_IRWXO  (S_IROTH|S_IWOTH|S_IXOTH)
    
#define S_IWRITE S_IWUSR
#define S_IREAD  S_IRUSR
#define S_IEXEC  S_IXUSR

#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000
    
#define S_IFMT   0170000
#define S_IFREG  0000000
#define S_IFDIR  0010000
#define S_IFCHR  0020000
#define S_IFBLK  0030000
#define S_IFIFO  0040000

#define __S_ISFMT(mode, type) (((mode) & S_IFMT) == (type))
#define S_ISREG(mode) __S_ISFMT(mode, S_IFREG)
#define S_ISDIR(mode) __S_ISFMT(mode, S_IFDIR)
#define S_ISCHR(mode) __S_ISFMT(mode, S_IFCHR)
#define S_ISBLK(mode) __S_ISFMT(mode, S_IFBLK)
#define S_ISFIFO(mode) __S_ISFMT(mode, S_IFIFO)

    int stat(const char *path, struct stat *buf) _IMPL("libc/unix/stat.c");
    int fstat(int fd, struct stat *buf);

#if defined(__cplusplus)
}
#endif

#endif
