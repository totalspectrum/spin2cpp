#ifndef _SYS_UNISTD_H
#define _SYS_UNISTD_H

#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif

  /* Standard file descriptors */
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

  typedef unsigned int useconds_t;

  int write(int fd, const void *buf, int count);
  int read(int fd, void *buf, int count);
  int close(int fd);
  off_t lseek(int fd, off_t offset, int whence);
  int isatty(int fd);

  char *getcwd(char *buf, int size);
  int chdir(const char *path);
  int rmdir(const char *path);

  unsigned int sleep(unsigned int seconds);
  int usleep(useconds_t usec);

  char *_mktemp(char *templ);
  char *mktemp(char *);

#if defined(__cplusplus)
}
#endif

#endif
