#ifndef test150_Class_Defined__
#define test150_Class_Defined__

#include <stdint.h>

class test150 {
public:
  static const char *	Ptr(const char *A);
  static const char *	Greet1(void);
  static const char *	Greet2(void);
  char *	Fluff(void);
private:
  volatile char 	Buf[10];
};

#endif
