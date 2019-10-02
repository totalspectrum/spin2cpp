#ifndef test146_Class_Defined__
#define test146_Class_Defined__

#include <stdint.h>

#ifndef Tuple2__
  struct tuple2__ { int32_t v0;  int32_t v1; };
# define Tuple2__ struct tuple2__
  static MakeTuple2__(int32_t x0,int32_t x1) {
    Tuple2__ t;
    t.v0 = x0;
    t.v1 = x1;
    return t;
  }
#endif

#ifndef Tuple3__
  struct tuple3__ { int32_t v0;  int32_t v1;  int32_t v2; };
# define Tuple3__ struct tuple3__
  static MakeTuple3__(int32_t x0,int32_t x1,int32_t x2) {
    Tuple3__ t;
    t.v0 = x0;
    t.v1 = x1;
    t.v2 = x2;
    return t;
  }
#endif

class test146 {
public:
  void 	Swapab(void);
  static Tuple3__ 	Seq3(int32_t N);
  void 	Setit(void);
private:
  int32_t 	A, B, C;
};

#endif
