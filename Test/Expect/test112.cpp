// test for org and label interaction
#include <propeller.h>
#include "test112.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

uint8_t test112::dat[] = {
  0x70, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x71, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x01, 0x00, 0x00, 0x00, 
};
int32_t test112::Me(void)
{
  return (*(int32_t *)&dat[12]);
}

