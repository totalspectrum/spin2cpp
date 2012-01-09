#ifndef test20_Class_Defined__
#define test20_Class_Defined__

#include <stdint.h>

class test20 {
public:
  static const int eol = 10;
private:
  int32_t	cog;
  int32_t	rx_head;
  int32_t	rx_tail;
  int32_t	tx_head;
  int32_t	tx_tail;
  int32_t	rx_pin;
  int32_t	tx_pin;
  int32_t	rxtx_mode;
  int32_t	bit_ticks;
  int32_t	buffer_ptr;
};

#endif
