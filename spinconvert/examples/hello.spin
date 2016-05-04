''
'' serial port demo
''
CON
  _clkfreq = 80_000_000
  _clkmode = xtal1 + pll16x
   
OBJ
 ser: "SimpleSerial"

PUB demo
  ser.start(31, 30, 0, 115200)
  repeat
    ser.str(string("hello, world!", 13, 10))

  

