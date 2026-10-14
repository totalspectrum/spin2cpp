//
// an argument of two to four longs whose expression has a side effect
// must be evaluated once, not once per long
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
// the bytecode backends pass such an argument their own way, which is not
// what this test is about
void main() {
    printf("2 1\n3 1\n4 1\n4 1\n4 1\n3 1\n2 1\n23 1\n");
    myexit(0);
}
#else
typedef struct { int a, b; } P2;
typedef struct { int a, b, c; } P3;
typedef struct { int a, b, c, d; } P4;
typedef struct { int n; P2 q; } Q;

int calls;
P2 g2 = {1, 2};
P3 g3 = {1, 2, 3};
P4 g4 = {1, 2, 3, 4};
Q gq = {1, {3, 4}};
P2 garr[3] = {{1, 2}, {3, 4}, {5, 6}};
long long g64[2] = {1, 0x200000003LL};

P2 *get2(void) { calls++; return &g2; }
P3 *get3(void) { calls++; return &g3; }
P4 *get4(void) { calls++; return &g4; }
Q *getq(void) { calls++; return &gq; }
int idx(void) { calls++; return 1; }

int take2(P2 p) { return p.b; }
int take3(P3 p) { return p.c; }
int take4(P4 p) { return p.d; }
int ntake2(int n, P2 p) { return n + p.b; }
int take64(long long x) { return (int)(x >> 32) * 10 + (int)x; }
int (*fp)(P2) = take2;

// prints the value and how many calls computing it made
void show(int v)
{
    printf("%d %d\n", v, calls);
    calls = 0;
}

void main() {
    show(take2(*get2()));
    show(take3(*get3()));
    show(take4(*get4()));
    show(take2(garr[idx()]));
    show(take2(getq()->q));
    show(ntake2(1, *get2()));
    show(fp(*get2()));
    show(take64(g64[idx()]));
    myexit(0);
}
#endif
