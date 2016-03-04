''
'' serial port demo
''
CON
  _clkfreq = 80_000_000
  _clkmode = xtal1 + pll16x
   
PUB demo | ptr, x, y
    ptr := @testvector
    repeat
      x := long[ptr]
      ptr += 4
      y := long[ptr]
      ptr += 4
      if (x == 0 and y == 0)
        quit
      testit(x, y)

    '' exit now
    ser_exit(0)

PRI testit(x, y)
  ser_str(string("x="))
  ser_dec(x)
  ser_str(string(" y="))
  ser_dec(y)
  ser_str(string(" x*y="))
  ser_dec(x*y)
  ser_str(string(" x/y="))
  ser_dec(x/y)
  ser_str(string(" x//y="))
  ser_dec(x//y)
  ser_str(string(13, 10))
  
#include "serial.def"

DAT

testvector
    long 5, 3
    long -101, 2
    long 6, -3
    long -8, -7
    long 0, 0