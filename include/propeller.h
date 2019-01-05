#ifndef _PROPELLER_H_
#define _PROPELLER_H_

#pragma once
#include <cog.h>
#include <stdint.h>

#ifdef __P2__
#define __PROPELLER2__
#endif

#ifdef __PROPELLER2__
#include <propeller2.h>
#else
#include <propeller1.h>
#endif

#define CLKFREQ _clkfreq

#endif
