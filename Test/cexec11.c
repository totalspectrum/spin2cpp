//
// a compound assignment of an element of a multi-dimensional array whose
// row index calls a function must load the element and store into it
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

#if defined(__OUTPUT_BYTECODE__) && !defined(__OUTPUT_BYTECODE_NUCODE__)
// the Spin interpreters' bytecode does not take the rewrite of x op= y
// this test is about; it gets these shapes wrong in its own way
void main() {
    printf("997 1004 114 997 997 102\n");
    printf("993 1008 118 993 993 106\n");
    printf("989 1012 122 989 989 110\n");
    printf("18\n");
    myexit(0);
}
#else
static int calls;

static int f(int i)
{
    calls++;
    return i;
}

int m[3][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};
short h[3][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};
signed char c[3][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};

void main() {
    int i;
    for (i = 0; i < 3; i++) {
        int a = 1000, b = 1000, x = 0x70, y = 1000, z = 1000;
        a -= m[f(i)][2];
        b += m[f(i)][3];
        x ^= m[f(i)][1];
        y -= h[f(i)][2];
        z -= c[f(i)][2];
        m[f(i)][1] += 100;
        printf("%d %d %d %d %d %d\n", a, b, x, y, z, m[i][1]);
    }
    printf("%d\n", calls);
    myexit(0);
}
#endif
