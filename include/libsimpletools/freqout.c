/*
 * @file freqout.c
 */
 
#include "simpletools.h" 

void freqout(int pin, int msTime, int frequency)
{

#ifdef __propeller2__

  uint32_t x = 1;
  uint32_t y = 0;

  // calculate appropriate y value based on frequency
  uint32_t temp = _clkfreq;
  __asm
  {
    qfrac frequency, temp
    getqx y
  }

  // for debugging
  //print("frequency = %d  y = %u clkfreq = %u \n", frequency, y, temp);

  _pinstart(pin, p_nco_freq | p_oe, x, y);

  pause(msTime);

  _pinclear(pin);

#else

  // Propeller 1
  int ctr, frq, channel;
  square_wave_setup(pin, frequency, &ctr, &frq);
  if(!CTRA)
  {
    channel = 0;
    FRQA = frq;
    CTRA = ctr;
    low(pin);
  }
  else
  {
    channel = 1;
    FRQB = frq;
    CTRB = ctr;
    low(pin);
  }
  pause(msTime);
  if(!channel)
  {
    FRQA = 0;
    CTRA = 0;
    input(pin);
  }
  else
  {
    FRQB = 0;
    CTRB = 0;
    input(pin);
  }

#endif

}
