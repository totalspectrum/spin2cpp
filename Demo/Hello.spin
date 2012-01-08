CON
  _clkmode = xtal1 + pll16x
  _xinfreq = 5_000_000

OBJ
  fds : "FullDuplexSerial.spin"

PUB demo | i

  '' start up the serial port
  fds.start(31, 30, 0, 115200)
  i := 1
  repeat
    fds.str(string("The count is: "))
    fds.dec(i++)
    fds.tx(13)
    fds.tx(10)
