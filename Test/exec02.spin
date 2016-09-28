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

  testdiv(1)
  testdiv(12345)
  testdiv(-1)
  testdiv(-999)
  testdiv($80000000)
  testdiv($80001234)
  exit

PUB newline
  fds.str(string(13, 10))

PUB exit
  fds.tx($ff)
  fds.tx($00)
  fds.tx($00) '' the exit status
  fds.txflush
  repeat

PRI printdec(x)
#ifdef __SPIN2CPP__
  '' the PropGCC compiler optimizer cannot
  '' handle 0x80000000 as we expect in fds.dec()
  if x == NEGX
    fds.str(string("-2147483648"))
  else
    fds.dec(x)
#else
  '' the SPIN2PASM processor should be able to handle it though
  fds.dec(x)
#endif

PUB testdiv(x)
  printdec(x)
  fds.str(string("/8 = "))
  fds.dec(x/8)
  newline
  printdec(x)
  fds.str(string("//16 = "))
  fds.dec(x//16)
  newline
  