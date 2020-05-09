/* functions for manipulating the environment */
/* written by Eric R. Smith and placed in the public domain */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

const char **_environ_ptr;
static int _environ_size; // max size of environment table

char *
getenv(const char *tag)
{
	char **var;
	char *name;
	size_t len = strlen(tag);

	if (!_environ_ptr)
            return 0;

	for (var = _environ_ptr; (name = *var) != 0; var++) {
		if (!strncmp(name, tag, len) && name[len] == '=')
			return name+len+1;
	}
	return 0;
}

int
putenv(const char *strng)
{
    size_t idx;
    const char **var;
    const char *name;
    size_t len = strlen(strng);
    
    if (!_environ_ptr) {
        name = 0;
        var = 0;
    } else {
        for (var = _environ_ptr; (name = *var) != 0; var++) {
            if (!strncmp(name, strng, len) && name[len] == '=')
                break;
        }
    }
    if (name) {
        // replace old value
        *var = strng;
        return 0;
    }
    idx = (var - _environ_ptr);
    var = realloc( _environ_ptr, (idx+2) * sizeof(char *) );
    if (!var) {
        return _seterror(ENOMEM);
    }
    var[idx] = strng;
    var[idx+1] = 0;
}
