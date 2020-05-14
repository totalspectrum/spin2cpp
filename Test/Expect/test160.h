#ifndef test160_Class_Defined__
#define test160_Class_Defined__

#include <stdint.h>

class test160 {
public:
  void 	Demo(void);
  static void 	Cfunc(int32_t X);
  void 	Blah(void);
  static int32_t 	Getfunc(int32_t X);
private:
  volatile int32_t 	A;
  volatile int32_t 	R;
  volatile int32_t 	Stack[32];
};

#endif
