/* from Henry Spencer's stringlib */
/* modified by ERS */

#include <string.h>
#include <ctype.h>
#include <compiler.h>

/*
 * strcasecmp - compare string s1 to s2, ignoring case
 */

int				/* <0 for <, 0 for ==, >0 for > */
strcasecmp(const char *scan1, const char *scan2)
{
	register int c1, c2;

	if (!scan1)
		return scan2 ? -1 : 0;
	if (!scan2) return 1;

	do {
	  c1 = toupper(*scan1++);
	  c2 = toupper(*scan2++);
	} while (c1 && c1 == c2);

	/*
	 * The following case analysis is necessary so that characters
	 * which look negative collate low against normal characters but
	 * high against the end-of-string NUL.
	 */
	// W21: only if char is signed (in flexspin it isn't)
#if 0
	if (c1 == c2)
		return(0);
	else if (c1 == '\0')
		return(-1);
	else if (c2 == '\0')
		return(1);
	else
#endif
		return(c1 - c2);
}
