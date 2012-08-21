#include <propeller.h>
#include "test60.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test60::Func(int32_t A, int32_t B)
{
  int32_t Ok = 0;
  if (A < B) {
    Ok = -1;
  } else {
    if (A == 0) {
      Ok = -1;
    } else {
      Ok = 0;
    }
  }
  return Ok;
}

