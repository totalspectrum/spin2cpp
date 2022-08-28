#include <stdio.h>
#include <propeller.h>
#undef printf

void myexit(int n)
{
    _txraw(0xff);
    _txraw(0x0);
    _txraw(n);
    waitcnt(getcnt() + 40000000);
#ifdef __OUTPUT_BYTECODE__
    _cogstop(1);
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
#warning 64 bit printf not working in bytecode yet
    __builtin_printf("x = 1122334455667788 y = aaccef10 : 22446688 z = 9abcdef01234\r\n");
#else
    long long x = 0x1122334455667788;
    long long y;
    y = x + x;
    printf("x = %llx y = %x : %x z = %llx\n", x, y, 0x9abcdef01234LL, 1);
#endif
    myexit(0);
}
