#ifndef test28_Class_Defined__
#define test28_Class_Defined__

#include <stdint.h>

class test28 {
public:
  int32_t	lock(void);
private:
  int32_t	thelock;
  int32_t	x;
};

#endif
