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

#undef	toupper /* note that in gcc we have a safe version of these, */
#undef  tolower	/* but its better to leave these as routines in case */
		/* some code uses these as function pointers --  i   */
		/* have seen code that does.			     */

/* table reduced to 129 entries by Eric Smith */

static unsigned char __ctype[129] =
	{
	_CTc, _CTc, _CTc, _CTc,				/* 0x00..0x03 */
	_CTc, _CTc, _CTc, _CTc,				/* 0x04..0x07 */
	_CTc, _CTc|_CTs|_CTb, _CTc|_CTs, _CTc|_CTs,     /* 0x08..0x0B */
	_CTc|_CTs, _CTc|_CTs, _CTc, _CTc,		/* 0x0C..0x0F */

	_CTc, _CTc, _CTc, _CTc,				/* 0x10..0x13 */
	_CTc, _CTc, _CTc, _CTc,				/* 0x14..0x17 */
	_CTc, _CTc, _CTc, _CTc,				/* 0x18..0x1B */
	_CTc, _CTc, _CTc, _CTc,				/* 0x1C..0x1F */

	_CTs|_CTb, _CTp, _CTp, _CTp,				/* 0x20..0x23 */
	_CTp, _CTp, _CTp, _CTp,				/* 0x24..0x27 */
	_CTp, _CTp, _CTp, _CTp,				/* 0x28..0x2B */
	_CTp, _CTp, _CTp, _CTp,				/* 0x2C..0x2F */

	_CTd|_CTx, _CTd|_CTx, _CTd|_CTx, _CTd|_CTx,	/* 0x30..0x33 */
	_CTd|_CTx, _CTd|_CTx, _CTd|_CTx, _CTd|_CTx,	/* 0x34..0x37 */
	_CTd|_CTx, _CTd|_CTx, _CTp, _CTp,		/* 0x38..0x3B */
	_CTp, _CTp, _CTp, _CTp,				/* 0x3C..0x3F */

	_CTp, _CTu|_CTx, _CTu|_CTx, _CTu|_CTx,		/* 0x40..0x43 */
	_CTu|_CTx, _CTu|_CTx, _CTu|_CTx, _CTu,		/* 0x44..0x47 */
	_CTu, _CTu, _CTu, _CTu,				/* 0x48..0x4B */
	_CTu, _CTu, _CTu, _CTu,				/* 0x4C..0x4F */

	_CTu, _CTu, _CTu, _CTu,				/* 0x50..0x53 */
	_CTu, _CTu, _CTu, _CTu,				/* 0x54..0x57 */
	_CTu, _CTu, _CTu, _CTp,				/* 0x58..0x5B */
	_CTp, _CTp, _CTp, _CTp,				/* 0x5C..0x5F */

	_CTp, _CTl|_CTx, _CTl|_CTx, _CTl|_CTx,		/* 0x60..0x63 */
	_CTl|_CTx, _CTl|_CTx, _CTl|_CTx, _CTl,		/* 0x64..0x67 */
	_CTl, _CTl, _CTl, _CTl,				/* 0x68..0x6B */
	_CTl, _CTl, _CTl, _CTl,				/* 0x6C..0x6F */

	_CTl, _CTl, _CTl, _CTl,				/* 0x70..0x73 */
	_CTl, _CTl, _CTl, _CTl,				/* 0x74..0x77 */
	_CTl, _CTl, _CTl, _CTp,				/* 0x78..0x7B */
	_CTp, _CTp, _CTp, _CTc,				/* 0x7C..0x7F */
	0
	};

#undef isupper

int isupper(int c)
{
  return __isctype(c, _CTu);
}

#undef islower

int islower(int c)
{
  return __isctype(c, _CTl);
}

#undef isspace

int isspace(int c)
{
  return __isctype(c, _CTs);
}

#undef isdigit

int isdigit(int c)
{
  return __isctype(c, _CTd);
}

#undef isalnum

int isalnum(int c)
{
  return __isctype(c, _CTalnum);
}

#undef isalpha

int isalpha(int c)
{
  return __isctype(c, _CTalpha);
}

#undef isblank

int isblank(int c)
{
  return __isctype(c, _CTb);
}

unsigned __ctype_get(unsigned int c)
{
    if (c > 128) c = 128;
    return __ctype[c];
}
