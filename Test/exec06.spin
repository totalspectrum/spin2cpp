CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
  fds : "FullDuplexSerial.spin"

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
  fds.txflush
  fds.stop
