#ifndef _STDIO_H_
#define _STDIO_H_

#include <compiler.h>

/* these are temporary defines */
#define putchar _tx
#define getchar _rx
#define printf __builtin_printf

char *gets(char *data) _IMPL("libc/stdio/gets.c");

#endif
