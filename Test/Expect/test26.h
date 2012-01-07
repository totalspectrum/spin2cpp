#ifndef test26_class_defined__
#define test26_class_defined__

#include <stdint.h>

class test26 {
public:
  int32_t	lock(void);
private:
  int32_t	thelock;
  int32_t	x;
};

#endif
