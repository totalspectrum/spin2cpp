#ifndef test80_Class_Defined__
#define test80_Class_Defined__

#include <stdint.h>
#include "FullDuplexSerial.h"

class test80 {
public:
  FullDuplexSerial	Fds;
  int32_t	Init(void);
private:
  uint8_t	Namebuffer[14];
};

#endif
