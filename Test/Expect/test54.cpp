#include <propeller.h>
#include "test54.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

uint8_t test54::dat[] = {
  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};
int32_t test54::Getfoo(void)
{
  int32_t result = 0;
  return (int32_t)(&(*(int32_t *)&dat[4]));
}

