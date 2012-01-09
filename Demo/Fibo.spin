CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
  fds : "FullDuplexSerial.spin"

PUB run | i,x,now,elapsed

  '' start up the serial port
  fds.start(31, 30, 0, 115200)

  '' and say hello
  repeat i from 10 to 20
    now := cnt
    x := fibo(i)
    elapsed := cnt - now
    fds.str(string("fibo("))
    fds.dec(i)
    fds.str(string(") = "))
    fds.dec(x)
    fds.str(string("  time taken: "))
    fds.dec(elapsed)
    fds.str(string(" cycles", 13, 10))

PUB fibo(x)
  if (x < 2)
    return x
  return fibo(x-1) + fibo(x-2)

