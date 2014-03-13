#ifndef test113_Class_Defined__
#define test113_Class_Defined__

#include <stdint.h>
#include "subtest113.h"

class test113 {
public:
  static const int X = 1;
  static const int Y = (X + 1);
  static const int Z = (subtest113::B + Y);
  subtest113	V;
  int32_t	Foo(int32_t Aa);
private:
};

#endif
