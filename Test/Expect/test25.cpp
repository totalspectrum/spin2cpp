#include <propeller.h>
#include "test25.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif

int32_t test25::Unlock(void)
{
  int32_t result = 0;
  if (X != 0) {
    return __extension__({ int32_t _tmp_ = X; X = 0; _tmp_; });
  }
}

