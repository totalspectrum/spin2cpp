#include <propeller.h>
#define RAND_MASK 0xffffff

static unsigned _seed = 0x1234567;
extern unsigned _lfsr_forward(unsigned);

// generate "truly" random bits
unsigned _randbits()
{
   unsigned r;
#ifdef __P2__
   __asm {
     getrnd r
   }
#else
  r = _lfsr_forward(_seed ^ _CNT);
  _seed = r;
#endif
  return (r>>4) & RAND_MASK;
}

// generate a "truly" random float
float _randfloat()
{
  unsigned r = _randbits();
  return ((float)r) / ((float)(RAND_MASK+1));
}

// BASIC rand(): generates a pseudo-random sequence
// if n < 0 then re-seed using n
// if n == 0 then re-seed randomly
// in all cases return next number in sequence
float _basic_rnd(int n)
{
    unsigned r;
    if (n < 0) {
        _seed = n;
    } else if (n == 0) {
        _seed = _getcnt();
    }
    r = _lfsr_forward(_seed);
    _seed = r;
    r = (r >> 4) & RAND_MASK;
    return ((float)r) / ((float)(RAND_MASK+1));
}
       
#ifdef TEST
#include <stdio.h>

int main()
{
  int i;
  float x;
  for (i = 0; i < 100; i++) {
    x = _randfloat();
    printf("%f (%08x)\n", x, x);
  }
}
#endif
