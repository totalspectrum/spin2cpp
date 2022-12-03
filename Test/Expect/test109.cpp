#define __SPIN2CPP__
#include <propeller.h>
#include "test109.h"

int32_t test109::Readdelta(int32_t Encid)
{
  int32_t Deltapos;
  Deltapos = 0 + (Encid < Totdelta);
  Totdelta = -(!Totdelta);
  return Deltapos;
}

