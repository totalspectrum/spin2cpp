''
'' blink LEDs in two other cogs
''
CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  pin1 = 15
  pin2 = 0
  pausetime = 40_000_000
  
OBJ
  fds: "FullDuplexSerial"
  blinka: "blinker.cog"
  blinkb: "blinker.cog"
  
PUB demo | id
  fds.start(31, 30, 0, 115200)
  id := blinka.__cognew
  blinka.run(pin1, pausetime)
  fds.str(string("blinka running in cog "))
  fds.dec(id)
  fds.tx(13)
  fds.tx(10)
  id := blinkb.__cognew
  blinkb.run(pin2, pausetime/2)
  fds.str(string("blinkb running in cog "))
  fds.dec(id)
  fds.tx(13)
  fds.tx(10)
