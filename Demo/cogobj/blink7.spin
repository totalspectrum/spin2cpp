'' blinkdemo.spin
'' blink 7 LEDs in other COGs
'' COGSPIN version
''
CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  basepin = 12
  basepause = 20_000_000
OBJ
  fds: "FullDuplexSerial"
  blink[7]: "blinker.cog"
  
PUB demo | id, i
  fds.start(31, 30, 0, 115200)
  repeat i from 0 to 7
    id := blink[i].__cognew
    blink.run(basepin + i, basepause * i)
    fds.str(string("blink running in cog "))
    fds.dec(id)
    fds.tx(13)
    fds.tx(10)
  repeat
