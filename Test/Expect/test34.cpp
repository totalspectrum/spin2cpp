#include <propeller.h>
#include "test34.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

uint8_t test34::dat[] = {
  0xf1, 0x03, 0xbc, 0xa2, 0x01, 0x02, 0x3c, 0xe3, 
};
void test34::Start(void)
{
  return;
}

