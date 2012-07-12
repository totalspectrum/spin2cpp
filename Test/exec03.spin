CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  twof = 2.0
  roottwof = ^^twof

OBJ
  fds: "FullDuplexSerial"

PUB demo | x,y

  fds.start(31, 30, 0, 115200)

  fds.str(string("2, sqrt(2)", 13, 10))

  fds.hex(twof, 8)
  fds.str(string(","))
  fds.hex(roottwof, 8)
  newline

  x := 15
  y := x + 2
  fds.str(string("now int sqrt:", 13, 10))
  fds.hex(y, 8)
  newline
  ^^y
  fds.hex(y, 8)
  newline
  exit

PUB newline
  fds.str(string(13, 10))

PUB exit
  waitcnt(cnt + 40000000)
  fds.stop
