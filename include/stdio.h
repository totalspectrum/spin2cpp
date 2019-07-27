#ifndef _STDIO_H_
#define _STDIO_H_

#include <compiler.h>

#ifndef EOF
#define EOF (-1)
#endif
#ifndef NULL
#define NULL ((void *)0)
#endif

/* these are temporary defines */
#define putchar(x) _tx(x)
#define getchar() _rx()
#define printf __builtin_printf

char *gets(char *data) _IMPL("libc/stdio/gets.c");

int sprintf(char *str, const char *format, ...) _IMPL("libc/stdio/sprintf.c");

#endif
