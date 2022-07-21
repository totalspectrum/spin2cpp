#include <stdio.h>
#include <propeller.h>
#undef printf

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

int main()
{
    long long x = 0x1122334455667788;
    long long y;
    y = x + x;
    printf("x = %llx y = %x : %x z = %llx\n", x, y, 0x9abcdef01234LL, 1);

    myexit(0);
}
