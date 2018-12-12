#ifdef USE_STDIO
#include <stdio.h>
#define println(x) printf(x "\n")
#define print(x) printf(x)
#else
struct __using("FullDuplexSerial.spin") fds;
#define println(x) fds.str(x "\r\n")
#define print(x) fds.str(x)
#define putchar(c) fds.tx(c)
#endif

enum MYKIND {
    BIT0 = 0x1,
    BIT1 = 0x2,
    BIT2 = 0x4,
};

void testswitch(int n)
{
    static int cntr = 1;
#ifdef USE_STDIO
    printf("call #%d: ", cntr++);
#else
    fds.str("call #");
    fds.dec(cntr++);
    fds.str(": ");
#endif
    switch(n) {
    case 0:
        println("zero");
        break;
    default:
        print("other ");
        // fall through
    case 1:
        println("one");
        break;
    }
}

void myexit(int n)
{
    putchar(0xff);
    putchar(0x0);
    putchar(n);
    __asm {
        cogid n
        cogstop n
    }
}

void main()
{
#ifndef USE_STDIO
    fds.start(31, 30, 0, 115200);
#endif
    testswitch(1);
    testswitch(0);
    testswitch(2);
    myexit(0);
}
