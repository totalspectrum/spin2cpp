#ifndef _STDIO_H_
#define _STDIO_H_

#include <compiler.h>
#include <sys/types.h>
#include <sys/limits.h>
#include <unistd.h>

#ifndef EOF
#define EOF (-1)
#endif
#ifndef NULL
#define NULL (0)
#endif
#ifndef FILENAME_MAX
#define FILENAME_MAX _PATH_MAX
#endif

typedef vfs_file_t FILE;

FILE *__getftab(int i) _IMPL("libc/unix/posixio.c");
#define stdin  __getftab(0)
#define stdout __getftab(1)
#define stderr __getftab(2)

#define putc(x, f) (((f)->putcf)( (x), (f) ))
#define getc(f)    (((f)->getcf)( (f) ))

int fputc(int c, FILE *f) _IMPL("libc/stdio/fputs.c");
int fgetc(FILE *f) _IMPL("libc/stdio/fputs.c");

#define putchar(x) fputc( (x), stdout)
#define getchar() fgetc(stdin)

int sprintf(char *str, const char *format, ...) _IMPL("libc/stdio/sprintf.c");
int printf(const char *format, ...) _IMPL("libc/stdio/fprintf.c");
int fprintf(FILE *f, const char *format, ...) _IMPL("libc/stdio/fprintf.c");

int vsprintf(char *str, const char *format, unsigned long ap) _IMPL("libc/stdio/sprintf.c");
int vprintf(const char *format, unsigned long ap) _IMPL("libc/stdio/fprintf.c");
int vfprintf(FILE *f, const char *format, unsigned long ap) _IMPL("libc/stdio/fprintf.c");

int fputs(const char *s, FILE *f) _IMPL("libc/stdio/fputs.c");
int puts(const char *s) _IMPL("libc/stdio/fputs.c");

char *gets(char *data) _IMPL("libc/stdio/gets.c");
char *fgets(char *buf, int size, FILE *f) _IMPL("libc/stdio/fgets.c");

FILE *fopen(const char *name, const char *mode) _IMPL("libc/stdio/fopen.c");
int fclose(FILE *f) _IMPL("libc/stdio/fopen.c");
int fflush(FILE *f) _IMPL("libc/stdio/fflush.c");
size_t fwrite(const void *ptr, size_t size, size_t n, FILE *f) _IMPL("libc/stdio/fwrite.c");
size_t fread(void *ptr, size_t size, size_t n, FILE *f) _IMPL("libc/stdio/fwrite.c");

void clearerr(FILE *f) _IMPL("libc/stdio/clearerr.c");
void perror(const char *s) _IMPL("libc/string/strerror.c");

int fileno(FILE *f) _IMPL("libc/stdio/fileno.c");

int  fseek(FILE *f, long offset, int whence) _IMPL("libc/stdio/fseek.c");
long ftell(FILE *f)                          _IMPL("libc/stdio/fseek.c");

#if defined(__FLEXC__) && !defined(_NO_BUILTIN_PRINTF)
// FLEXC can optimize printf
#define printf __builtin_printf
#endif

#define feof(f)   (0 != ((f)->state & _VFS_STATE_EOF))
#define ferror(f) (0 != ((f)->state & _VFS_STATE_ERR))

int rename(const char *oldpath, const char *newpath) _IMPL("libc/unix/rename.c");

#endif
