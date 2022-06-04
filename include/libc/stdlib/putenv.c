/* functions for manipulating the environment */
/* written by Eric R. Smith and placed in the public domain */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

int
putenv(const char *strng)
{
    size_t idx;
    const char **var;
    const char *name;
    size_t len = strlen(strng);
    char **environ_ptr = _get_environ_ptr();
    
    if (!environ_ptr) {
        name = 0;
        var = 0;
    } else {
        for (var = environ_ptr; (name = *var) != 0; var++) {
            if (!strncmp(name, strng, len) && name[len] == '=')
                break;
        }
    }
    if (name) {
        // replace old value
        *var = strng;
        return 0;
    }
    idx = (var - environ_ptr);
    var = realloc( environ_ptr, (idx+2) * sizeof(char *) );
    if (!var) {
        return _seterror(ENOMEM);
    }
    var[idx] = strng;
    var[idx+1] = 0;
    _put_environ_ptr(var);
    return 0;
}
