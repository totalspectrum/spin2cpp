#include <stdio.h>

enum MYKIND {
    BIT0 = 0x1,
    BIT1 = 0x2,
    BIT2 = 0x4,
};

void testswitch(int n)
{
    switch(n) {
    case 0:
        printf("zero\n");
        break;
    default:
        printf("other ");
        // fall through
    case 1:
        printf("one\n");
        break;
    }
}

void myexit(int n)
{
    putchar(0xff);
    putchar(0x0);
    putchar(n);
    __asm {
        cogid n
        cogstop n
    }
}

void main()
{
    testswitch(1);
    testswitch(0);
    testswitch(2);
    myexit(0);
}
   
