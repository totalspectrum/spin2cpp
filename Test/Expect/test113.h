#ifndef test113_Class_Defined__
#define test113_Class_Defined__

#include <stdint.h>
#include "subtest113.h"

class test113 {
public:
  static const int Z = (subtest113::B + Y);
  static const int X = 1;
  static const int Y = (X + 1);
  static int32_t 	Foo(int32_t Aa);
private:
  subtest113 	V;
};

#endif
