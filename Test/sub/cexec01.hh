#include <propeller.h>
#include <compiler.h>

#ifdef USE_STDIO
#include <stdio.h>
#define println(x) printf(x "\n")
#define print(x) printf("%s", x)
#define printdec(x) printf("%d", x)
#define printhex(x) printf("%x", x)
#else
# ifdef __P2__
extern struct __using("spin/SmartSerial.spin") fds;
# else
extern struct __using("spin/FullDuplexSerial.spin") fds;
#endif
#define println(x) fds.str(x "\r\n")
#define print(x) fds.str(x)
#define putchar(c) fds.tx(c)
#define printdec(x) fds.dec(x)
#define printhex(x) fds.hex(x,8)
#endif


void external_func(void) _IMPL("sub/cexec_helper.c");
static void internal_func(void);
