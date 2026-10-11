//
// a conditional instruction that sets flags must not be folded
// as if it always ran (flexprop issue 118)
//
#include <stdio.h>
#include <propeller.h>

int g;          // 0
int seed = 5;

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
    int v = seed;
    _Bool f = (v < v);       // false, and the optimizer can tell
    int r = (12 != g) || f;  // 1, since g is 0
    _Bool f2 = (seed > 100); // false, and the optimizer cannot tell
    int r2 = (12 != g) || f2;
    printf("%d %d\n", r, r2);
    myexit(0);
}
