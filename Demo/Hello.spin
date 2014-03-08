CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
  fds : "FullDuplexSerial.spin"

PUB demo | i

  '' start up the serial port
  fds.start(31, 30, 0, 115200)

  '' and say hello
  repeat 3
    fds.str(string("Hello, world!", 13, 10))

