#define __SPIN2CPP__
#include <propeller.h>
#include "test033.h"

int32_t test033::Byteextend(int32_t X)
{
  int32_t 	_temp__0000;
  _temp__0000 = ((int32_t)X << 24) >> 24;
  X = _temp__0000;
  return _temp__0000;
}

int32_t test033::Wordextend(int32_t X)
{
  int32_t 	_temp__0000;
  _temp__0000 = ((int32_t)X << 16) >> 16;
  X = _temp__0000;
  return _temp__0000;
}

