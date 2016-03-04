''
'' serial port demo
''
CON
  _clkfreq = 80_000_000
  _clkmode = xtal1 + pll16x
   
PUB demo
  repeat 4
    ser_str(string("hello, world!", 13, 10))
  repeat
  
#include "serial.def"
