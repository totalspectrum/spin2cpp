'' check for whether DAT sections are shared between objects
'' (they should be)

CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
  fds : "FullDuplexSerial.spin"

OBJ
  A : "dattest.spin"
  B : "dattest.spin"

PUB main | n
  '' start up the serial port
  fds.start(31, 30, 0, 115200)

  A.setvar(1)
  n := A.getvar
  fds.str(string("expect 1 got "))
  print(n)
  n := B.getvar
  fds.str(string("expect 1 got "))
  print(n)
  B.setvar(2)
  n := A.getvar
  fds.str(string("expect 2 got "))
  print(n)
  exit

PUB exit
'' send an exit sequence which propeller-load recognizes:
'' FF 00 xx, where xx is the exit status
''
  fds.tx($ff)
  fds.tx($00)
  fds.tx($00) '' the exit status
  fds.txflush
  repeat


PUB print(n)
  fds.str(string("n="))
  fds.hex(n, 8)
  fds.str(string(13,10))
