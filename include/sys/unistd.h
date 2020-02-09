#ifndef _SYS_UNISTD_H
#define _SYS_UNISTD_H

#include <sys/types.h>
#include <compiler.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* Standard file descriptors */
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define _MAX_FILES 10
    
  typedef unsigned int useconds_t;

  int open(const char *name, int flags, mode_t mode) _IMPL("libc/unix/posixio.c");
  int write(int fd, const void *buf, int count) _IMPL("libc/unix/posixio.c");
  int read(int fd, void *buf, int count) _IMPL("libc/unix/posixio.c");
  int close(int fd) _IMPL("libc/unix/posixio.c");
  off_t lseek(int fd, off_t offset, int whence) _IMPL("libc/unix/posixio.c");
  int isatty(int fd);

  char *getcwd(char *buf, int size);
  int chdir(const char *path);
  int rmdir(const char *path);
  int mkdir(const char *path, int mode);

  unsigned int sleep(unsigned int seconds) _IMPL("libc/time/sleep.c");
  int usleep(useconds_t usec) _IMPL("libc/time/usleep.c");

  char *_mktemp(char *templ);
  char *mktemp(char *);

#if defined(__cplusplus)
}
#endif

#endif
