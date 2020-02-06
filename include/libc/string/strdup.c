/*
 * strdup: return a duplicate of a string
 * Written by Eric R. Smith and placed in the public domain.
 */

#include <stdlib.h>
#include <string.h>
#undef strdup

char *
strdup(s)
	const char *s;
{
	char *dup;

	dup = (char *) malloc(strlen(s)+1);
	if (dup)
		strcpy(dup, s);
	return dup;
}
