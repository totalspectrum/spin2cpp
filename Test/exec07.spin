CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
  fds : "FullDuplexSerial.spin"

PUB demo | n, i

  '' start up the serial port
  fds.start(31, 30, 0, 115200)

  n := getvar
  fds.str(string("n="))
  fds.hex(n, 8)
  fds.str(string(13,10))
  exit

PUB exit
  waitcnt(cnt + 40000000)
  fds.stop

PUB getvar
  return myvar

DAT

myvar long $11223344
