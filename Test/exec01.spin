CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
  fds: "FullDuplexSerial"

PUB demo | x,y

  fds.start(31, 30, 0, 115200)

  fds.str(string("Forward test", 13, 10))

  x := 1
  putnum(x?)
  putnum(x?)
  putnum(?x)
  putnum(?x)

  fds.str(string("Backward test", 13, 10))
  x := 1
  putnum(?x)
  putnum(?x)
  exit

PUB exit
'' send an exit sequence which propeller-load recognizes:
'' FF 00 xx, where xx is the exit status
''
  fds.tx($ff)
  fds.tx($00)
  fds.tx($00) '' the exit status
  fds.txflush
  repeat
  
PUB putnum(x)
  fds.dec(x)
  fds.str(string(13, 10))
