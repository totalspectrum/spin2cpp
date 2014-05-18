#include <propeller.h>
#include "test60.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test60::Func(int32_t A, int32_t B)
{
  int32_t Ok = 0;
  if (A < B) {
    Ok = -1;
  } else {
    // if more digits, add current digit and prepare next
    if (A == 0) {
      Ok = -1;
    } else {
      // if no more digits, add "0"
      Ok = 0;
    }
  }
  return Ok;
}

