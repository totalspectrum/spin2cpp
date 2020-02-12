#include <stdio.h>

int fileno(FILE *f)
{
    return (f - stdin);
}
