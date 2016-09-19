CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  twof = 2.0
  roottwof = ^^twof

OBJ
  fds: "FullDuplexSerial"

PUB demo | x,y,z,i

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

  i := 1
  z := (x:=i++) + (y:=i++)
  fds.str(string("expr eval: x="))
  fds.hex(x,4)
  fds.str(string(" y="))
  fds.hex(y,4)
  fds.str(string(" z="))
  fds.hex(z,4)
  newline

  i := 100
  info(++i, ++i)
  exit

PUB info(x,y)
  fds.str(string("func eval: x="))
  fds.hex(x,4)
  fds.str(string(" y="))
  fds.hex(y,4)
  newline

PUB newline
  fds.str(string(13, 10))

PUB exit
'' send an exit sequence which propeller-load recognizes:
'' FF 00 xx, where xx is the exit status
''
  fds.tx($ff)
  fds.tx($00)
  fds.tx($00) '' the exit status
  fds.txflush
  repeat
