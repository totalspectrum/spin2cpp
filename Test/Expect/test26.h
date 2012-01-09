#ifndef test26_Class_Defined__
#define test26_Class_Defined__

#include <stdint.h>

class test26 {
public:
  int32_t	lock(void);
private:
  int32_t	thelock;
  int32_t	x;
};

#endif
