/**
 * @file include/sys/ctype.h
 * @brief Defines macros and types used by include/ctype.h
 */
/*
 * doxygen note.
 * \@file uses include/sys/ctype.h to differentiate from include/ctype.h
 */

#ifndef _SYS_CTYPE_H
#define _SYS_CTYPE_H

#ifdef __FLEXC__
unsigned __ctype_get(unsigned c) __fromfile("libc/misc/ctype.c");
#else
/* internal definitions for ctype.h and wctype.h */
/* we have an array of 129 bytes (0..128) */
extern unsigned char __ctype[];

#if defined(__GNUC__)
#define __ctype_get(c) __extension__({unsigned int _i_ = c; if (_i_ > 128) _i_ = 128; __ctype[_i_];}) 
#else
#define __ctype_get(c) __ctype[((unsigned int)(c) > 128 ? 128 : (c))]
#endif
#endif

#define _CTc    0x01            /* control character */
#define _CTd    0x02            /* numeric digit */
#define _CTu    0x04            /* upper case */
#define _CTl    0x08            /* lower case */
#define _CTs    0x10            /* whitespace */
#define _CTp    0x20            /* punctuation */
#define _CTx    0x40            /* hexadecimal */
#define _CTb    0x80            /* "blank" (space or horizontal tab) */

#define _CTYPE_T unsigned short

/* the upper 8 bits of the wctype_t are not actually stored in the
 * table, but rather denote various special conditions
 */
#define _CTmask   0x00ff

#define _CTanybut 0x4000        /* character has some bit set other than the ones specified in the lower 8 bits */
#define _CTnot    0x8000        /* invert condition in the remaining bits */

/* some useful combinations */
#define _CTalnum (_CTu|_CTl|_CTd)
#define _CTalpha (_CTu|_CTl)
#define _CTgraph (_CTanybut|(_CTc|_CTs))
#define _CTprint (_CTanybut|_CTc)

#endif
