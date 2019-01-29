/* from Henry Spencer's stringlib */
#include <string.h>
#undef strlen

/*
 * strlen - length of string (not including NUL)
 */
size_t
strlen(const char *scan)
{
	const char *start = scan+1;

	if (!scan) return 0;
	while (*scan++ != '\0')
		continue;
	return (size_t)((long)scan - (long)start);
}
