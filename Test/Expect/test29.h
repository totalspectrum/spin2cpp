#ifndef test29_Class_Defined__
#define test29_Class_Defined__

#include <stdint.h>

class test29 {
public:
  int32_t	tx(int32_t val);
  int32_t	str(int32_t stringptr);
private:
  int32_t	strlock;
  int32_t	idx;
};

#endif
