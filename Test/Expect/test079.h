#ifndef test079_Class_Defined__
#define test079_Class_Defined__

#include <stdint.h>

class test079 {
public:
// i2c driver
  static const int Scl = 28;
  static const int Sda = 29;
  static void	I2c_start(void);
private:
};

#endif
