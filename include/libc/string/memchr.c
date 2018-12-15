/* from Henry Spencer's stringlib */

#include <string.h>

/*
 * memchr - search for a byte
 */

void *
memchr(const void *s, int ucharwanted, size_t size)
{
	register const char *scan;
	register size_t n;

	scan = (const char *) s;
	for (n = size; n > 0; n--)
		if (*scan == (char) ucharwanted)
			return((void *)scan);
		else
			scan++;

	return(NULL);
}
