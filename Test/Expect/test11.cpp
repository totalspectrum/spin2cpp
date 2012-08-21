#include <propeller.h>
#include "test11.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test11::Peek(void)
{
  int32_t result = 0;
  return Count;
}

int32_t test11::Donext(void)
{
  int32_t result = 0;
  Count = (Peek() + 1);
  return Count;
}

