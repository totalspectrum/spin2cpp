CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
  fds : "FullDuplexSerial.spin"

VAR
    long x

PUB demo

  '' start up the serial port
  fds.start(31, 30, 0, 115200)

  x.long[0] := $11223344
  x.byte[1] := $aa
  fds.str(string("x="))
  fds.hex(x, 8)
  fds.str(string(13,10))
  fds.txflush
  fds.stop

