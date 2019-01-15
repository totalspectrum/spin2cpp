CON
#ifdef __P2__
  _clkmode = $010c3f04
  _clkfreq = 160_000_000
#else
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
#endif

VAR
  long SqStack[64]	' Stack space for Square cog
  
OBJ
#ifdef __P2__
  ser: "SmartSerial.spin2"
#else
  ser: "FullDuplexSerial"
#endif

PUB demo | x,y

#ifdef __P2__
  clkset(_clkmode, _clkfreq)
  ser.start(63, 62, 0, 2000000)
#else
  ser.start(31, 30, 0, 115200)
#endif

  x := 2
  cognew(Square(@X), @SqStack)
  repeat 7
    Report(x)
    PauseABit
  exit

PUB Report(n)
  ser.str(string("x = "))
  ser.dec(n)
  ser.str(string(13, 10))

PUB Square(XAddr)
 ' Square the value at XAddr
 repeat
   long[XAddr] *= long[XAddr]
   waitcnt(80_000_000 + cnt)
   
PUB exit
'' send an exit sequence which propeller-load recognizes:
'' FF 00 xx, where xx is the exit status
''
  ser.tx($ff)
  ser.tx($00)
  ser.tx($00) '' the exit status
#ifndef __P2__
  ser.txflush
#endif
  repeat

PUB PauseABit
  waitcnt(40_000_000 + cnt)
