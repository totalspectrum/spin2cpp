#ifndef test128_Class_Defined__
#define test128_Class_Defined__

#include <stdint.h>

class test128 {
public:
  void	Demo(void);
  static void	Square(int32_t Xaddr);
private:
// Stack space for Square cog
  volatile int32_t	Sqstack[6];
};

#endif
