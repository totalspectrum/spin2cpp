#pragma once

#include <stdint.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define INCLUDE_FLOATS
#undef _SIMPLE_IO

#ifdef __FLEXC__

#define SMALL_INT
#define strlen __builtin_strlen
#define strcpy __builtin_strcpy
#ifdef _SIMPLE_IO
#define THROW_RETURN(err) return -1
#else
#define THROW_RETURN(x) do { __throwifcaught(x); return -1; } while (0)
#endif
#include <compiler.h>

#else

#define THROW_RETURN(x) return -1
#include <string.h>

#endif

#define alloca(x) __builtin_alloca(x)

#include <sys/fmt.h>

#define DEFAULT_PREC 6
#define DEFAULT_BASIC_FLOAT_FMT ((1<<UPCASE_BIT)|((4+1)<<PREC_BIT))
#define DEFAULT_FLOAT_FMT ((1<<UPCASE_BIT))

