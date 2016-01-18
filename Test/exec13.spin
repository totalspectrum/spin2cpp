CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

VAR
  long SqStack[6]	' Stack space for Square cog
  
OBJ
  fds: "FullDuplexSerial"

PUB demo | x,y

  fds.start(31, 30, 0, 115200)

  x := 2
  cognew(Square(@X), @SqStack)
  repeat 7
    Report(x)
    PauseABit
  exit

PUB Report(n)
  fds.str(string("x = "))
  fds.dec(n)
  fds.str(string(13, 10))

PUB Square(XAddr)
 ' Square the value at XAddr
 repeat
   long[XAddr] *= long[XAddr]
   waitcnt(80_000_000 + cnt)
   
PUB exit
  fds.txflush
  fds.stop

PUB PauseABit
  waitcnt(40_000_000 + cnt)
