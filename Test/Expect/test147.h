#ifndef test147_Class_Defined__
#define test147_Class_Defined__

#include <stdint.h>

#ifndef Tuple2__
  struct tuple2__ { int32_t v0;  int32_t v1; };
# define Tuple2__ struct tuple2__
  static Tuple2__ MakeTuple2__(int32_t x0,int32_t x1) {
    Tuple2__ t;
    t.v0 = x0;
    t.v1 = x1;
    return t;
  }
#endif

class test147 {
public:
  static Tuple2__ 	Dbl64(int32_t Ahi, int32_t Alo);
  static Tuple2__ 	Quad64(int32_t Ahi, int32_t Alo);
private:
};

#endif
