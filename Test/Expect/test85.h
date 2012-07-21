#ifndef test85_Class_Defined__
#define test85_Class_Defined__

#include <stdint.h>
#include "FullDuplexSerial.h"
#include "objarrtest.h"

class test85 {
public:
  static const int _clkmode = 1032;
  static const int _clkfreq = 80000000;
  static const int Max_obj = 3;
  FullDuplexSerial	Fds;
  objarrtestSpin	V[3];
  int32_t	Main(void);
private:
  int32_t	Printn(int32_t I);
};

#endif
