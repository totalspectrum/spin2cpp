#include <propeller.h>
#include "test14.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

uint8_t test14::dat[] = {
  0x01, 0x00, 0x00, 0x00, 0xdd, 0xcc, 0xbb, 0xaa, 
};
int32_t test14::Myfunc(void)
{
  return (*(int32_t *)&dat[4]);
}

