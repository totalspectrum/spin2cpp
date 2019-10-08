/* from Henry Spencer's stringlib */
/* check for src==NULL added by ERS */

#include <string.h>
#undef strcat

/*
 * strcat - append string src to dst
 */
char *				/* dst */
strcat(dst, src)
char *dst;
const char *src;
{
	register char *dscan;
	register const char *sscan;

	if ((sscan = src) != NULL)
	{
	    for (dscan = dst; *dscan != '\0'; dscan++)
		continue;
	    while ((*dscan++ = *sscan++) != '\0')
			continue;
	}
	return(dst);
}
