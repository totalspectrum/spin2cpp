#ifndef _MATH_H
#define _MATH_H

#pragma once

#ifdef __FLEXC__
#define abs(x) __builtin_abs(x)
#define sqrt(x) __builtin_sqrt(x)
#endif

#endif
