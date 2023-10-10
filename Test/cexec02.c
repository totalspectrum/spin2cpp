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

void trybool(int *xp) {
    _Bool a = !xp;
    _Bool b = xp;
    int i;

    for (i = 0; i < 4; i++) {
        printf("a = %d b=%d\n", a, b);
        a++;
        b--;
    }
}

//
// declaring main like this should pull in the c_start routine,
// which should be harmless for tests
//
int main(int argc, char **argv)
{
    long long x = 0x1122334455667788;
    long long y;
    int z;
    y = x + x;
    printf("x = %llx y = %x : %x z = %llx\n", x, y, 0x9abcdef01234LL, 1);
    trybool(&z);
    myexit(0);
    return 0;
}
