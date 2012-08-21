#include <propeller.h>
#include "test48.h"

#ifdef __GNUC__
#define INLINE__ static inline
#else
#define INLINE__ static
#endif
INLINE__ int32_t PostFunc__(int32_t *x, int32_t y) { int32_t t = *x; *x = y; return t; }
#define PostEffect__(X, Y) PostFunc__(&(X), (Y))

int32_t test48::Setcolors(int32_t Colorptr)
{
  int32_t	I, Fore, Back;
  int32_t result = 0;
  I = 0;
  do {
    Fore = (((uint8_t *)Colorptr)[(I << 1)] << 2);
    _OUTA = ((_OUTA & 0xffffff00) | ((Fore & 0xff) << 0));
    I = (I + 1);
  } while (I <= 7);
  return result;
}

int32_t test48::Stop(void)
{
  int32_t result = 0;
  _OUTA &= ~(255<<0);
  return result;
}

