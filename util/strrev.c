//
// string reverser
// Written by Eric R. Smith (ersmith@totalspectrum.ca)
// and placed in the public domain
//

#include <stdio.h>
#include <ctype.h>
#include <string.h>

//
// reverse a string in-place
//
char* _strrev(char *origstr)
{
    char *str = origstr;
    char *end;
    int c;

    for (end = str; *end; end++) ;
    --end;
    while (end > str) {
        c = *str;
        *str++ = *end;
        *end-- = c;
    }
    return origstr;
}
