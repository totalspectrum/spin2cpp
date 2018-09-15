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
  float 	z_R;
  float 	w_R;
};

#endif
