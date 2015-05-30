#ifndef test119_Class_Defined__
#define test119_Class_Defined__

#include <stdint.h>

class test119 {
public:
  static const int Lcd_w = 96;
  static const int Width = (Lcd_w / 32);
  int32_t	Clear(void);
private:
  int32_t	Array[Width];
};

#endif
