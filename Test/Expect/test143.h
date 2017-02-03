#ifndef test143_Class_Defined__
#define test143_Class_Defined__

#include <stdint.h>

class test143 {
public:
  void 	Putval(void);
private:
  int32_t *volatile 	Ptr;
  volatile int32_t 	Data[10];
};

#endif
