#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define S_IWUSR   0400
#define S_IRUSR   0200
#define S_IXUSR   0100

#define S_IWRITE S_IWUSR
#define S_IREAD  S_IRUSR
#define S_IEXEC  S_IXUSR

#define S_IFMT   07000
#define S_IFREG  00000
#define S_IFDIR  01000
#define S_IFCHR  02000
#define S_IFBLK  03000
#define S_IFIFO  04000

#define __S_ISFMT(mode, type) (((mode) & S_IFMT) == (type))
#define S_ISREG(mode) __S_ISFMT(mode, S_IFREG)
#define S_ISDIR(mode) __S_ISFMT(mode, S_IFDIR)
#define S_ISCHR(mode) __S_ISFMT(mode, S_IFCHR)
#define S_ISBLK(mode) __S_ISFMT(mode, S_IFBLK)
#define S_ISFIFO(mode) __S_ISFMT(mode, S_IFIFO)

int mkdir(const char *path, int mode);
int stat(const char *path, struct stat *buf);
int fstat(int fd, struct stat *buf);

#if defined(__cplusplus)
}
#endif

#endif
