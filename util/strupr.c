//
// make a string upper case
// Written by Eric R. Smith (ersmith@totalspectrum.ca)
// and placed in the public domain
//

#include <stdio.h>
#include <ctype.h>
#include <string.h>

//
// make a string upper case
//
char* _strupr(char *origstr)
{
    char *str = origstr;
    int c;
    while ( (c = *str) != 0 ) {
        if (islower(c)) {
            *str = toupper(c);
        }
        str++;
    }
    return origstr;
}
