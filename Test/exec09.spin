''
'' serial port demo
''
CON
#ifdef __P2__
  _clkmode = $010c3f04
  _clkfreq = 160_000_000
#else
  _clkfreq = 80_000_000
  _clkmode = xtal1 + pll16x
#endif

VAR
  long i
  
OBJ
#ifdef __P2__
 ser: "SmartSerial.spin2"
#else
 ser: "FullDuplexSerial"
#endif

PUB demo
#ifdef __P2__
  clkset(_clkmode, _clkfreq)
  ser.start(63, 62, 0, 230400)
#else
  ser.start(31, 30, 0, 115200)
#endif
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
