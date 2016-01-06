#ifndef test94_Class_Defined__
#define test94_Class_Defined__

#include <stdint.h>
#include "userdef.h"

class test94 {
public:
  static const int Myticks = (userdef::Ticks + 1);
  userdef	U;
  static int32_t	Foo(void);
private:
};

#endif
