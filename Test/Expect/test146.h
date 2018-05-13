#ifndef test146_Class_Defined__
#define test146_Class_Defined__

#include <stdint.h>

#ifndef Tuple2__
  struct tuple2__ { int32_t v0;  int32_t v1; };
# define Tuple2__ struct tuple2__
#endif

#ifndef Tuple3__
  struct tuple3__ { int32_t v0;  int32_t v1;  int32_t v2; };
# define Tuple3__ struct tuple3__
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
