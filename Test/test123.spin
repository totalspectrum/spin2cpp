CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

VAR
  long Stack[64]	' Stack space for Square cog
  
OBJ
  fds: "FullDuplexSerial"

PUB demo

  cognew(Hello, @Stack)
  PauseABit
  PauseABit
  exit

PRI Hello
  fds.str(string("Hello, World", 13, 10))
   
PUB exit
  fds.txflush
  fds.stop

PUB PauseABit
  waitcnt(40_000_000 + cnt)
