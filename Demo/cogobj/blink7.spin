''
'' blink LEDs in multiple COGs
''
CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  basepin = 10
  basepause = 10_000_000
  numcogs = 6 '' leave room for FDS and Spin
  
OBJ
  fds: "FullDuplexSerial"
  blink[numcogs]: "blinker.cog"
  
PUB demo | id, i
  fds.start(31, 30, 0, 115200)
  repeat i from 0 to numcogs-1
    id := blink[i].__cognew
    blink[i].run(basepin + i, basepause * i)
    fds.str(string("blink running in cog "))
    fds.dec(id)
    fds.tx(13)
    fds.tx(10)
  repeat
