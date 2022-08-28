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
    waitcnt(getcnt() + 40000000);
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
#ifdef __OUTPUT_BYTECODE__
#warning 64 bit integers not working in bytecode yet
    __builtin_printf("x = 1122334455667788 y = aaccef10 : 22446688 z = 9abcdef01234\n");
#else    
    long long x = 0x1122334455667788;
    long long y;
    y = x + x;
    printf("x = %llx y = %x : %x z = %llx\n", x, y, 0x9abcdef01234LL, 1);
#endif
    myexit(0);
}
