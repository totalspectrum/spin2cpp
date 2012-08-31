#include <propeller.h>
#include "test13.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))
#endif

uint8_t test13::dat[] = {
  0xaa, 0xbb, 0xcc, 0x00, 0xdd, 0xcc, 0xbb, 0xaa, 
};
