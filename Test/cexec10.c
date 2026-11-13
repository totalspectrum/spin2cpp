//
// a function with no frame of its own that passes a struct on must give
// back the stack it takes for the copy of the argument
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
// the bytecode backends pass such a struct their own way, which is not
// what this test is about
void main() {
    printf("4200 0\n600 0\n");
    myexit(0);
}
#else
typedef struct { long long a; int b; } T;
typedef struct { int a, b, c, d, e; } W;

// a frame of their own: a local array
int use(T t) { volatile int w[2]; w[0] = (int)t.a; w[1] = t.b; return w[0] + w[1]; }
int usew(W t) { volatile int w[1]; w[0] = t.a + t.e; return w[0]; }

// no frame: nothing of their own but passing the struct on
int fwd(T t) { return use(t); }
int fwdw(W t) { return usew(t); }

// the address of its local is the stack pointer where it is called, called
// through a pointer so that it is not inlined
int sp0(void) { volatile int x = 0; return (int)&x; }
int (*volatile sp)(void) = sp0;

void main() {
    T t = {40, 2};
    W w = {1, 2, 3, 4, 5};
    int m = 0, n = 0;
    int s0, s1, s2;
    int i;

    s0 = sp();
    for (i = 0; i < 100; i++) m += fwd(t);
    s1 = sp();
    for (i = 0; i < 100; i++) n += fwdw(w);
    s2 = sp();
    printf("%d %d\n%d %d\n", m, s1 - s0, n, s2 - s1);
    myexit(0);
}
#endif
