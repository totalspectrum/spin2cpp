#include <propeller.h>
#include "test33.h"

#ifdef __GNUC__
#define INLINE__ static inline
#define PostEffect__(X, Y) __extension__({ int32_t tmp__ = (X); (X) = (Y); tmp__; })
#else
#define INLINE__ static
static int32_t tmp__;
#define PostEffect__(X, Y) (tmp__ = (X), (X) = (Y), tmp__)
#endif

int32_t test33::Byteextend(int32_t X)
{
  int32_t result = 0;
  return ((X << 24) >> 24);
}

int32_t test33::Wordextend(int32_t X)
{
  int32_t result = 0;
  return ((X << 16) >> 16);
}

