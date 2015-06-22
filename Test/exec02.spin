CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  twof = -2.0
  abstwof = ||twof

OBJ
  fds: "FullDuplexSerial"

PUB demo | x,y

  fds.start(31, 30, 0, 115200)

  fds.str(string("2, abs(2)", 13, 10))

  fds.hex(twof, 8)
  fds.str(string(","))
  fds.hex(abstwof, 8)
  newline

  x := -15
  y := x + 2
  fds.str(string("now int abs:", 13, 10))
  fds.dec(y)
  newline
  ||y
  fds.dec(y)
  newline
  exit

PUB newline
  fds.str(string(13, 10))

PUB exit
  fds.txflush
  fds.stop

