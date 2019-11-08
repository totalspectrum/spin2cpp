#include <stddef.h>

typedef struct foo {
    unsigned short S;
    unsigned long L;
} Foo;

size_t getoff1()
{
    Foo *T;
    T = (Foo *)0;
    return (size_t)(&T->L);
}
size_t getoff2()
{
    return (size_t)(&(((Foo *)0)->L));
}
