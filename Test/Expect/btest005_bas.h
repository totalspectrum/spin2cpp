#ifndef btest005_Class_Defined__
#define btest005_Class_Defined__

#include <stdint.h>

class btest005 {
public:
  static const int lopin = 16;
  static const int mscycles = 80000;
  static void 	pausems(uint32_t ms);
  static void 	program(void);
private:
};

#endif
