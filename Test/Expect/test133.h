#ifndef test133_Class_Defined__
#define test133_Class_Defined__

#include <stdint.h>

class test133 {
public:
  static void	Putx(int32_t C);
  static int32_t	Start(int32_t A);
private:
  int32_t	Baud;
  int32_t	Txmask;
  int32_t	Bitcycles;
  char	Txpin;
  char	Rxpin;
};

#endif
