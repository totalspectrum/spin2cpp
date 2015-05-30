#ifndef test05_Class_Defined__
#define test05_Class_Defined__

#include <stdint.h>

class test05 {
public:
  static const int Size = 4;
private:
  int32_t	X[Size];
  uint8_t	Str[Size];
};

#endif
