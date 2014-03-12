#include <propeller.h>
#include "test26.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define Yield__() __asm__ volatile( "" ::: "memory" )
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#undef lockclr /* work around a bug in propgcc */
#define lockclr(i) __asm__ volatile( "  lockclr %[_id]" : : [_id] "r"(i) :)
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#define Yield__()
#endif

int32_t test26::Lock(void)
{
  while (lockset(Thelock) != 0) {
    Yield__();
  }
  X = 0;
  while (X < 4) {
    lockret(X);
    (X++);
  }
  return 0;
}

