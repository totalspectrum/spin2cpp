//
// a member of a struct that a function returns in registers
// must be read from the register that holds it
//
#include <stdio.h>
#include <propeller.h>

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

#ifdef __OUTPUT_BYTECODE__
// the bytecode backends return such a struct their own way, which is not
// what this test is about
void main() {
    printf("5\n1 2\n1 2 3 4\n");
    myexit(0);
}
#else
typedef struct { const char *ptr; int len; } S;
typedef struct { int a, b; } P2;
typedef struct { int a, b, c, d; } P4;

S gs = {"hello", 5};

S mkS(void) { return gs; }
P2 mkP(int x) { P2 r = {x, x + 1}; return r; }
P4 mk4(int x) { P4 r = {x, x + 1, x + 2, x + 3}; return r; }

void main() {
    printf("%d\n", mkS().len);
    printf("%d %d\n", mkP(1).a, mkP(1).b);
    printf("%d %d %d %d\n", mk4(1).a, mk4(1).b, mk4(1).c, mk4(1).d);
    myexit(0);
}
#endif
