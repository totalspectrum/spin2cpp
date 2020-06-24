#ifndef btest003_Class_Defined__
#define btest003_Class_Defined__

#include <stdint.h>
#include "FullDuplexSerial_spin.h"

class btest003 {
public:
  void 	program(void);
private:
  FullDuplexSerial_spin 	ser;
  char 	i;
};

#endif
