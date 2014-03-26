#include <propeller.h>
#include "test23.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#undef lockclr /* work around a bug in propgcc */
#define lockclr(i) __asm__ volatile( "  lockclr %[_id]" : : [_id] "r"(i) :)
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test23::Start(void)
{
  int32_t	X;
  int32_t R = 0;
  // 
  //  just a comment
  // 
  X = locknew();
  R = lockclr(X);
  return R;
}

