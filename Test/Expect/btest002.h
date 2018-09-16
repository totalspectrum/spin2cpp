#ifndef btest002_Class_Defined__
#define btest002_Class_Defined__

#include <stdint.h>

class btest002 {
public:
  void 	init(void);
  static uint32_t 	getcycles(void);
private:
  int32_t 	x;
  int32_t 	y;
  int32_t 	z_R;
  int32_t 	w_R;
};

#endif
