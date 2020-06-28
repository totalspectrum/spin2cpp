#ifndef btest003_Class_Defined__
#define btest003_Class_Defined__

#include <stdint.h>
#include "FullDuplexSerial.h"

class btest003 {
public:
  void 	program(void);
private:
  FullDuplexSerial 	ser;
  char 	i;
};

#endif
