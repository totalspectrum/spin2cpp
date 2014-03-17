// ********************************************
// *  Full-Duplex Serial Driver v1.2          *
// *  Author: Chip Gracey, Jeff Martin        *
// *  Copyright (c) 2006-2009 Parallax, Inc.  *
// *  See end of file for terms of use.       *
// ********************************************
/* -----------------REVISION HISTORY-----------------
 v1.2 - 5/7/2009 fixed bug in dec method causing largest negative value (-2,147,483,648) to be output as -0.
 v1.1 - 3/1/2006 first official release.
 */
#include <propeller.h>
#include "test20.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

