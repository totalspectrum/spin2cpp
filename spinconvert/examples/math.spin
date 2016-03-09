''
'' serial port demo
''
CON
  _clkfreq = 80_000_000
  _clkmode = xtal1 + pll16x

OBJ
  ser: "SimpleSerial"
''  ser: "FullDuplexSerial" '' use this with openspin
  
PUB demo | ptr, x, y
    ser.start(31, 30, 0, 115200)
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
    repeat

PRI testit(x, y)
  ser.str(string("x="))
  ser.dec(x)
  ser.str(string(" y="))
  ser.dec(y)
  ser.str(string(" x*y="))
  ser.dec(x*y)
  ser.str(string(" x/y="))
  ser.dec(x/y)
  ser.str(string(" x//y="))
  ser.dec(x//y)
  ser.str(string(" x><y="))
  ser.dec(x><y)
  ser.str(string(13, 10))
  
DAT

testvector
    long 5, 3
    long -101, 2
    long $12345678, 12
    long 6, -3
    long -8, -7
    long 0, 0