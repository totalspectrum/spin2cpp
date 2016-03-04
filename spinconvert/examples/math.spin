''
'' serial port demo
''
CON
  _clkfreq = 80_000_000
  _clkmode = xtal1 + pll16x
   
PUB demo
    testit(5, 3)
    testit(-101, 2)
    testit(6, -3)
    testit(-40, -8)
    repeat      

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
