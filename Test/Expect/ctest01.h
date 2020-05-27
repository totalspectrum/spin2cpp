#ifndef ctest01_Class_Defined__
#define ctest01_Class_Defined__

#include <stdint.h>


typedef struct ctest01 {
  int32_t 	Cntr;
} ctest01;

  void ctest01_add(ctest01 *self, int32_t x);
  void ctest01_inc(ctest01 *self);
  void ctest01_dec(ctest01 *self);
  int32_t ctest01_Get(ctest01 *self);
  int32_t ctest01_Double(int32_t x);
#endif
