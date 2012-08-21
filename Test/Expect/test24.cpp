#include <propeller.h>
#include "test24.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test24::Unlock(void)
{
  int32_t result = 0;
  return __extension__({ int32_t _tmp_ = X; X = 0; _tmp_; });
}

