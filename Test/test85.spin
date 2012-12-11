CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  MAX_OBJ = 3

OBJ
  fds : "FullDuplexSerial"
  v[MAX_OBJ]: "objarrtest"

PUB main
  fds.start(31, 30, 0, 115200)
  fds.str(string("object array test", 13, 10))
  printn(0)
  printn(1)
  printn(2)
  fds.str(string("increment v[0]", 10, 13))
  v.incn  '' should be the same as v[0].incn
  printn(0)
  printn(1)
  printn(2)


PRI printn(i) | r
  fds.str(string("v["))
  fds.dec(i)
  fds.str(string("] = "))
  r := v[i].getn
  fds.dec(r)
  fds.str(string(10, 13))
