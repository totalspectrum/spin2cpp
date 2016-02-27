#include <stdlib.h>
#include <propeller.h>
#include "test40.h"

void test40::Tx(int32_t Character)
{
  OUTA = Character;
}

int32_t test40::Dec(int32_t Value)
{
  int32_t	I, X, _idx__0000;
  int32_t result = 0;
  // Print a decimal number
  // Check for max negative
  X = -(Value == (int32_t)0x80000000U);
  if (Value < 0) {
    // If negative, make positive; adjust for max negative
    Value = abs((Value + X));
    // and output sign
    Tx('-');
  }
  // Initialize divisor
  I = 1000000000;
  for(_idx__0000 = 0; _idx__0000 < 10; _idx__0000++) {
    // Loop for 10 digits
    if (Value >= I) {
      // If non-zero digit, output digit; adjust for max negative
      Tx((((Value / I) + '0') + (X * -(I == 1))));
      // and digit from value
      Value = Value % I;
      // flag non-zero found
      result = -1;
    } else {
      if ((result) || (I == 1)) {
        // If zero digit (or only digit) output it
        Tx('0');
      }
    }
    // Update divisor
    I = I / 10;
  }
  return result;
}

