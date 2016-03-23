''
'' serial port demo
''
CON
  _clkfreq = 80_000_000
  _clkmode = xtal1 + pll16x
   
OBJ
 ser: "FullDuplexSerial"

PUB demo | i, c
  ser.start(31, 30, 0, 115200)
  repeat i from 0 to 5
    c := \mytest(i)
    if c <> 0
      ser.str(string("abort "))
      ser.dec(c)
      ser.str(string(" called", 13, 10))
  repeat
  
pub mytest(i)
  result := 0
  ser.str(string("testing: "))
  ser.dec(i)
  ser.str(string(" -> "))
  testfunc(i)
  ser.str(string(13,10))

pub testfunc(i)
  if (i == 3)
    abort 88
  i := i*i
  ser.dec(i)
