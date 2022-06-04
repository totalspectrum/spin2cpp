/* functions for manipulating the environment */
/* written by Eric R. Smith and placed in the public domain */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static const char **_environ_ptr;
static int _environ_size; // max size of environment table

const char **_get_environ_ptr() {
    return _environ_ptr;
}

void _put_environ_ptr(const char **ptr) {
    _environ_ptr = ptr;
}

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
