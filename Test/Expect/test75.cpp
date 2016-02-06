#include <propeller.h>
#include "test75.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

uint8_t test75::dat[] = {
  0x00, 0xec, 0xff, 0xa0, 
};
void test75::Start(int32_t Code)
{
  int32_t _local__0000[5];
  cognew((int32_t)(&(*(int32_t *)&dat[0])), (int32_t)(&_local__0000[1 + 0]));
}

