''
'' serial port demo
''
CON
  _clkfreq = 80_000_000
  _clkmode = xtal1 + pll16x

VAR
  long i
  
OBJ
 ser: "FullDuplexSerial"

PUB demo
  ser.start(31, 30, 0, 115200)
  repeat i from 0 to 5
    if not \mytest
      ser.str(string("abort called", 13, 10))
  exit
  
PUB exit
'' send an exit sequence which propeller-load recognizes:
'' FF 00 xx, where xx is the exit status
''
  ser.tx($ff)
  ser.tx($00)
  ser.tx($00) '' the exit status
  ser.txflush
  repeat

pub mytest
  result := true
  ser.str(string("testing: "))
  ser.dec(i)
  ser.str(string(" -> "))
  testfunc(i)
  ser.str(string(13,10))

pub testfunc(i)
  if (i == 3)
    abort 0
  i := i*i
  ser.dec(i)
