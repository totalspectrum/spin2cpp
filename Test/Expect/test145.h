#ifndef test145_Class_Defined__
#define test145_Class_Defined__

#include <stdint.h>

class test145 {
public:
  void 	Main(void);
  void 	Incr(int32_t X);
  void 	Incr1(void);
  static void 	Sfunc(int32_t X);
  static void 	Subfunc(test145 *self, int32_t X);
private:
  volatile int32_t 	Stack[4];
  volatile int32_t 	Counter;
};

#endif
