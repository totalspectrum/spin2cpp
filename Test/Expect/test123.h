#ifndef test123_Class_Defined__
#define test123_Class_Defined__

#include <stdint.h>
#include "FullDuplexSerial.h"

class test123 {
public:
  static const int _Clkmode = (8 + 1024);
  static const int _Clkfreq = 80000000;
  FullDuplexSerial	Fds;
  void	Demo(void);
  void	Exit(void);
  static void	Pauseabit(void);
private:
// Stack space for Square cog
  volatile int32_t	Stack[64];
  static void	Hello(test123 *self);
};

#endif
