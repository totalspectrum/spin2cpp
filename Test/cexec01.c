#include <propeller.h>

#ifdef USE_STDIO
#include <stdio.h>
#define println(x) printf(x "\n")
#define print(x) printf("%s", x)
#define printdec(x) printf("%d", x)
#else
struct __using("FullDuplexSerial.spin") fds;
#define println(x) fds.str(x "\r\n")
#define print(x) fds.str(x)
#define putchar(c) fds.tx(c)
#define printdec(x) fds.dec(x)
#endif

enum MYKIND {
    BIT0 = 0x1,
    BIT1 = 0x2,
    BIT2 = 0x4,
};

void testswitch(int n)
{
    static int cntr = 1;
    print("call #");
    printdec(cntr++);
    print(": ");
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
    waitcnt(getcnt() + 80000000);
    __asm {
        cogid n
        cogstop n
    }
}

struct blah {
    const char *name;
    short num;
} mytab[3] = {
    { "the first string", 1 },
    { "the second string", 2 },
    { 0, 0 },
};

void main()
{
#ifndef USE_STDIO
    fds.start(31, 30, 0, 115200);
#endif
    testswitch(1);
    testswitch(0);
    testswitch(2);

    for (int i = 0; ; i++) {
        if (!mytab[i].name) break;
        printdec(i);
        print(": ");
        print(mytab[i].name);
        println("");
    }
    myexit(0);
}
