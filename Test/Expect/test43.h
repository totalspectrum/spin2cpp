#ifndef test43_Class_Defined__
#define test43_Class_Defined__

#include <stdint.h>
#include "test01.h"

class test43 {
public:
  static const int Cols = 40;
  static const int Rows = 12;
  static const int Screensize = 480;
  static const int Lastrow = 440;
  test01	A;
  int32_t	Getx(void);
private:
};

#endif
