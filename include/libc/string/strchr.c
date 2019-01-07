/* from Henry Spencer's stringlib */
#include <string.h>

/*
 * strchr - find first occurrence of a character in a string
 */

char *				/* found char, or NULL if none */
strchr(const char *s, int charwanted)
{
	char c;

	/*
	 * The odd placement of the two tests is so NUL is findable.
	 */
	while ((c = *s++) != (char) charwanted)
		if (c == 0) return NULL;
	return((char *)--s);
}
