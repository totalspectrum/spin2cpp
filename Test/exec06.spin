CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
  fds : "spin/FullDuplexSerial"

PUB demo | n, i

  '' start up the serial port
  fds.start(31, 30, 0, 115200)

  n := 0
  repeat n
    fds.str(string("loop 1", 13, 10))
  repeat i from n to 1
    fds.str(string("loop 1b", 13, 10))
  n := 4
  repeat n
    fds.str(string("loop 2", 13, 10))
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

