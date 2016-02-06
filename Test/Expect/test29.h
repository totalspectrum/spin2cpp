#ifndef test29_Class_Defined__
#define test29_Class_Defined__

#include <stdint.h>

class test29 {
public:
  void	Tx(int32_t Val);
  void	Str(int32_t Stringptr);
private:
  int32_t	Strlock;
  int32_t	Idx;
};

#endif
