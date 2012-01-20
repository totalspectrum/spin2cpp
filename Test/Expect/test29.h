#ifndef test29_Class_Defined__
#define test29_Class_Defined__

#include <stdint.h>

class test29 {
public:
  int32_t	Tx(int32_t Val);
  int32_t	Str(int32_t Stringptr);
private:
  int32_t	Strlock;
  int32_t	Idx;
};

#endif
