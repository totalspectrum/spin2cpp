#include <propeller.h>
#include <compiler.h>

#ifdef USE_STDIO
#include <stdio.h>
#define println(x) printf(x "\n")
#define print(x) printf("%s", x)
#define printdec(x) printf("%d", x)
#else
extern struct __using("spin/FullDuplexSerial.spin") fds;
#define println(x) fds.str(x "\r\n")
#define print(x) fds.str(x)
#define putchar(c) fds.tx(c)
#define printdec(x) fds.dec(x)
#endif


void external_func(void) _IMPL("sub/cexec_helper.c");
static void internal_func(void);
