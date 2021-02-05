/* from Henry Spencer's stringlib */
#include <string.h>

/*
 * strrchr - find last occurrence of a character in a string
 */

char *				/* found char, or NULL if none */
strrchr(const char *s, int charwanted)
{
	int c;
	const char *place;

	place = NULL;
	while ((c = *s++) != 0)
		if (c == charwanted)
			place = s - 1;
	if (charwanted == '\0')
		place = (--s);
	return (char *)place;
}
