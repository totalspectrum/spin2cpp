#ifndef Ctest01_Class_Defined__
#define Ctest01_Class_Defined__

#include <stdint.h>


typedef struct Ctest01 {
  int32_t	Cntr;
} Ctest01;

  int32_t	Ctest01_add( Ctest01 *self, int32_t x);
  int32_t	Ctest01_inc( Ctest01 *self );
  int32_t	Ctest01_dec( Ctest01 *self );
  int32_t	Ctest01_Get( Ctest01 *self );
  int32_t	Ctest01_ctest01( Ctest01 *self );
#endif
