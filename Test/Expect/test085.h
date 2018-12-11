#ifndef test085_Class_Defined__
#define test085_Class_Defined__

#include <stdint.h>
#include "FullDuplexSerial.h"
#include "objarrtest.h"

class test085 {
public:
  static const int _Clkmode = (8 + 1024);
  static const int _Clkfreq = 80000000;
  static const int Max_obj = 3;
  void 	Main(void);
private:
  FullDuplexSerial 	Fds;
  objarrtest 	V[Max_obj];
  void 	Printn(int32_t I);
};

#endif
