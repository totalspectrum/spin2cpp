#ifndef test043_Class_Defined__
#define test043_Class_Defined__

#include <stdint.h>
#include "test001.h"

class test043 {
public:
  static const int Cols = 40;
  static const int Rows = 12;
  static const int Screensize = (Cols * Rows);
  static const int Lastrow = (Screensize - Cols);
  int32_t 	Getx(void);
private:
  test001 	A;
};

#endif
