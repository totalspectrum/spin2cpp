/*
 * note, this header file may be included multiple times, with and without
 * _NEED_WINT_T defined first
 * if _NEED_WINT_T is defined, we define wint_t
 *
 * for gcc, the default for wchar_t is "int" and for wint_t is "unsigned int"
 */

#ifndef _WCHAR_T_DEFINED
#ifndef _WCHAR_T_TYPE
#define _WCHAR_T_TYPE int
#endif
#define _WCHAR_T_DEFINED _WCHAR_T_TYPE
//typedef _WCHAR_T_DEFINED wchar_t;
typedef int wchar_t;
#endif

#if defined(_NEED_WINT_T) && !defined(_WINT_T_DEFINED)
#define _WINT_T_DEFINED unsigned int
typedef _WINT_T_DEFINED wint_t;
#endif

#if !defined(_MBSTATE_T_DEFINED)
typedef struct _Mbstate {
  unsigned int total:5;    /* total bytes in character */
  unsigned int left:5;     /* number of bytes remaining in the character */
  unsigned int partial:22; /* partial wide character constructed/output */
} _Mbstate_t;

#define _MBSTATE_T_DEFINED _Mbstate_t
#endif
