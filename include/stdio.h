#ifndef _STDIO_H_
#define _STDIO_H_

#include <compiler.h>

#ifndef EOF
#define EOF (-1)
#endif
#ifndef NULL
#define NULL ((void *)0)
#endif

typedef struct FILE {
    int (*putc)(int c);
    int (*getc)();
    int (*closef)();
} FILE;

FILE *__getftab(int i) _IMPL("libc/stdio/ftab.c");
#define stdin  __getftab(0)
#define stdout __getftab(1)
#define stderr __getftab(2)

#define fputc(x, f) (((f)->putc)(x))
#define fgetc(f)    (((f)->getc)())
#define putchar(x) fputc(x, stdout)
#define getchar() fgetc(stdin)

char *gets(char *data) _IMPL("libc/stdio/gets.c");

int sprintf(char *str, const char *format, ...) _IMPL("libc/stdio/sprintf.c");
int printf(const char *format, ...) _IMPL("libc/stdio/fprintf.c");
int fprintf(FILE *f, const char *format, ...) _IMPL("libc/stdio/fprintf.c");

int fputs(const char *s, FILE *f) _IMPL("libc/stdio/fputs.c");
int puts(const char *s) _IMPL("libc/stdio/fputs.c");

#ifdef __FLEXC__
// FLEXC can optimize printf
#define printf __builtin_printf
#endif

#endif
