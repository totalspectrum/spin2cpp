#ifndef test28_class_defined__
#define test28_class_defined__

#include <stdint.h>

class test28 {
public:
  int32_t	lock(void);
private:
  int32_t	thelock;
  int32_t	x;
};

#endif
