#ifndef _STDIO_H_
#define _STDIO_H_

#include <compiler.h>
#include <sys/types.h>

#ifndef EOF
#define EOF (-1)
#endif
#ifndef NULL
#define NULL ((void *)0)
#endif

typedef struct vfs_file_t FILE;

FILE *__getftab(int i) _IMPL("libc/unix/posixio.c");
#define stdin  __getftab(0)
#define stdout __getftab(1)
#define stderr __getftab(2)

// FIXME: fputc and fgetc should be functions, not macros
#define fputc(x, f) (((f)->putcf)( (x), (f) ))
#define fgetc(f)    (((f)->getcf)( (f) ))

#define putc(x, f) (((f)->putcf)( (x), (f) ))
#define getc(f)    (((f)->getcf)( (f) ))

#define putchar(x) fputc( (x), stdout)
#define getchar() fgetc(stdin)

int sprintf(char *str, const char *format, ...) _IMPL("libc/stdio/sprintf.c");
int printf(const char *format, ...) _IMPL("libc/stdio/fprintf.c");
int fprintf(FILE *f, const char *format, ...) _IMPL("libc/stdio/fprintf.c");

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

#ifdef __FLEXC__
// FLEXC can optimize printf
#define printf __builtin_printf
#endif

#define feof(f)   (0 != ((f)->state & _VFS_STATE_EOF))
#define ferror(f) (0 != ((f)->state & _VFS_STATE_ERR))

#endif
