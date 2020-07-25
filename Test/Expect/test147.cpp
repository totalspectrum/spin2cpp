#include <propeller.h>
#include "test147.h"

Tuple2__ test147::Dbl64(int32_t Ahi, int32_t Alo)
{
  int32_t Bhi;  int32_t Blo;
  Bhi = Ahi + Ahi;
  Blo = Alo + Alo;
  if (Alo & 0x80000000) {
    (Bhi++);
  }
  return MakeTuple2__(Bhi, Blo);
}

// function to quadruple a 64 bit number
Tuple2__ test147::Quad64(int32_t Ahi, int32_t Alo)
{
  int32_t 	_parm__0000, _parm__0001;
  { Tuple2__ tmp__ = Dbl64(Ahi, Alo); _parm__0000 = tmp__.v0; _parm__0001 = tmp__.v1;  };
  return Dbl64(_parm__0000, _parm__0001);
}

