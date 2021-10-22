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
#define __SPIN2CPP__
#include <propeller.h>
#include "test020.h"

