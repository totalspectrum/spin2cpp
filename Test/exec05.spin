CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
  fds : "FullDuplexSerial.spin"

PUB demo

  '' start up the serial port
  fds.start(31, 30, 0, 115200)

  fds.str(string("Counting demo:", 13, 10))
  count(1,4,1)
  count(2,8,2)
  count(2,9,2)
  count(9,2,2)
  count(1,4,-1)
  count(4,0,-1)
  count(-4,0,1)
  count(-4,0,-1)
  exit

PUB count(a,b,c) | i
  fds.str(string("count "))
  fds.dec(a)
  fds.str(string(" to "))
  fds.dec(b)
  fds.str(string(" step "))
  fds.dec(c)
  fds.str(string(": "))
  repeat i from a to b step c
    fds.dec(i)
    fds.str(string(" "))
  fds.str(string(13,10))

PUB exit
'' send an exit sequence which propeller-load recognizes:
'' FF 00 xx, where xx is the exit status
''
  fds.tx($ff)
  fds.tx($00)
  fds.tx($00) '' the exit status
  fds.txflush
  repeat
