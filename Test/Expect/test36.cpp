#include <propeller.h>
#include "test36.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

uint8_t test36::dat[] = {
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 
  0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 
};
int32_t test36::Start(void)
{
  int32_t result = 0;
  return result;
}

