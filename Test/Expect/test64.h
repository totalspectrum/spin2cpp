#ifndef test64_Class_Defined__
#define test64_Class_Defined__

#include <stdint.h>

class test64 {
public:
  static const int X = 3;
  static const int Y = (X * X);
  static char dat[];
  static int32_t *	Getit(void);
private:
};

#endif
