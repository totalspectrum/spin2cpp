#ifndef test134_Class_Defined__
#define test134_Class_Defined__

#include <stdint.h>

class test134 {
public:
  static const int Test_row_results = 5;
  static const int Test_electrical = 0;
  static const int Test_light = 1;
  static const int Test_led = 2;
  static const int Test_row_button = (Test_row_results + Test_led);
  static unsigned char dat[];
private:
};

#endif
