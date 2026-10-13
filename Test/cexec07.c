//
// a counting loop whose limit changes inside it must read the limit on
// every pass (flexprop issue 119)
//
#include <stdio.h>
#include <propeller.h>

int gn = 5;
int gm = 5;

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

void lower(void) { gm = 3; }

void main() {
    int c = 0, d = 0, e = 0, f = 0, i, n = 5, k = 5;
    for (i = 0; i < n; i++) { // a local limit the body lowers
        c++;
        if (c == 2) n = 3;
    }
    for (i = 0; i < gn; i++) { // a global limit the body lowers
        d++;
        if (d == 2) gn = 3;
    }
    for (i = 0; i < gm; i++) { // a global limit a called function lowers
        e++;
        if (e == 2) lower();
    }
    for (i = 0; i < k; i++) { // a local limit the body leaves alone
        f++;
    }
    printf("%d %d %d %d\n", c, d, e, f);
    myexit(0);
}
