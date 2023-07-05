/* from dLibs v1.2, by Dale Schumacher */
/* 
  I'd like to see these routines distributed as widely as possible as I
feel they will be a benefit to many people.  The only restriction I'd
like to place on the use of dLibs is this.  If you modify the source
code, please don't redistribute it under the name "dLibs".  I encourage
people to look at, play with, modify and improve the source code as
much as they want, and would gladly accept suggestions (preferrably
with source code) for changes/additions to dLibs.  I simply want to
avoid the problem of digressive versions of dLibs.
--
      Dale Schumacher                         399 Beacon Ave.
      (alias: Dalnefre')                      St. Paul, MN  55104
      dal@syntel.UUCP                         United States of America
    "It's not reality that's important, but how you perceive things."
*/

/*
 *  CTYPE.C	Character classification and conversion
 */

#include <ctype.h>
#include <limits.h>

#undef	toupper /* note that in gcc we have a safe version of these, */
#undef  tolower	/* but its better to leave these as routines in case */
		/* some code uses these as function pointers --  i   */
		/* have seen code that does.			     */

int
tolower(int c)
{
  //return isupper(c) ? (c ^ 0x20) : c;
  return (unsigned)c - 'A' < 26u ? (c ^ 0x20) : c;
  
}
