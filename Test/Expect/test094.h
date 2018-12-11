#ifndef test094_Class_Defined__
#define test094_Class_Defined__

#include <stdint.h>
#include "userdef.h"

class test094 {
public:
  static const int Myticks = (userdef::Ticks + 1);
  static int32_t 	Foo(void);
private:
  userdef 	U;
};

#endif
