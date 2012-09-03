#include <propeller.h>
#include "test64.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

uint8_t test64::dat[] = {
  0x00, 0x00, 0x10, 0x41, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 
};
int32_t test64::Getit(void)
{
  int32_t result = 0;
  return (int32_t)(&(*(int32_t *)&dat[0]));
}

