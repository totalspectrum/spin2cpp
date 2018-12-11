#ifndef test080_Class_Defined__
#define test080_Class_Defined__

#include <stdint.h>
#include "FullDuplexSerial.h"

class test080 {
public:
  void 	Init(void);
private:
  FullDuplexSerial 	Fds;
  char 	Namebuffer[14];
};

#endif
