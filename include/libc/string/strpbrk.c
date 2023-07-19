/* from Henry Spencer's stringlib */
#include <string.h>

/*
 * strpbrk - find first occurrence of any char from breakat in s
 */

char *				/* found char, or NULL if none */
strpbrk(const char *s, const char *breakat)
{
	register const char *sscan;
	register const char *bscan;

	for (sscan = s; *sscan != '\0'; sscan++) {
		for (bscan = breakat; *bscan != '\0';)	/* ++ moved down. */
			if (*sscan == *bscan++)
				return((char *)sscan);
	}
	return(NULL);
}
