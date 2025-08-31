#include <stdio.h>
#include <propeller.h>
#undef printf /* in case it was defined to __builtin_printf */

typedef struct sx1268_handle_s {
    void (*debug_print)(const char *const fmt, ...);
} handle;

handle foo;

void myexit(int n)
{
    putchar(0xff);
    putchar(0x0);
    putchar(n);
    waitcnt(getcnt() + 40000000);
#ifdef __OUTPUT_BYTECODE__
    _cogstop(_cogid());
#else
    __asm {
        cogid n
        cogstop n
    }
#endif    
}

void main() {
    foo.debug_print = (void *)printf;
    foo.debug_print("hello, world\n");
    foo.debug_print("number: %d\n", 1);
    foo.debug_print("tuple: %d, %s\n", 2, "here");
    myexit(0);
}
