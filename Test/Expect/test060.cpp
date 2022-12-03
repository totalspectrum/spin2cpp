#define __SPIN2CPP__
#include <propeller.h>
#include "test060.h"

int32_t test060::Func(int32_t A, int32_t B)
{
  int32_t Ok;
  if (A < B) {
    Ok = -1;
  } else {
    // if more digits, add current digit and prepare next
    if (A == 0) {
      Ok = -1;
    } else {
      // if no more digits, add "0"
      Ok = 0;
    }
  }
  return Ok;
}

