''
'' blink an LED in another COG
'' COGSPIN version
''
CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
  fds: "FullDuplexSerial"
  blink: "blinker.cog"
  
PUB demo | id, count
  fds.start(31, 30, 0, 115200)
  count := 0
  id := blink.__cognew
  blink.run(@count)
  fds.str(string("blink running in cog "))
  fds.dec(id)
  fds.tx(13)
  fds.tx(10)
  waitcnt(CNT+80_000_000)
  fds.str(string("blinked "))
  fds.dec(count)
  fds.str(string(" times", 13, 10))

