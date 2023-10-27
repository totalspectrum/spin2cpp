#ifndef test064_Class_Defined__
#define test064_Class_Defined__

#include <stdint.h>

class test064 {
public:
  static const int X = 3;
  static const int Y = (X * X);
  static unsigned char dat[];
  static int32_t *	Getit(void);
private:
};

#endif
