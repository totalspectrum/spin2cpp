#include "cexec01.h"

struct __using("spin/FullDuplexSerial.spin") fds;

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

void msg(const char *msg, int n)
{
    print(msg);
    printdec(n);
    println("");
}

void fhello(int n)
{
    msg("hello ", n);
}
static void fgoodbye(int n)
{
    msg("goodbye ", n);
}
typedef void (*Fptr)(int);

struct blah {
    const char *name;
    Fptr func;
    short num;
} mytab[3] = {
    { "the first string", &fhello, 1 },
    { "the second string", &fgoodbye, -2 },
    { 0, 0, 0 },
};

static void
internal_func(void)
{
    println("main internal function");
}

void main()
{
    int v = 1;
    
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
        print("->");
        (*mytab[i].func)(mytab[i].num);
    }
    print("top level v=");
    printdec(v);
    if (1) {
        int v;
        v = 2;
        print(" nested v=");
        printdec(v);
    }
    print(" and top level v=");
    printdec(v);
    println("");
    internal_func();
    external_func();
    
    myexit(0);
}
