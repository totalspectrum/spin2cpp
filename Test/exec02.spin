CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  twof = -2.0
  abstwof = ||twof

OBJ
  fds: "spin/FullDuplexSerial"

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

  fds.str(string("fibo1(7)="))
  fds.dec(fibo1(7))
  fds.str(string(13, 10))
  fds.str(string("fibo2(7)="))
  fds.dec(fibo2(7))
  fds.str(string(13, 10))

  fds.str(string("<=> tests", 13, 10))
  fds.dec(fibo2(3) <=> 1)
  fds.str(string(13,10))
  fds.dec(2 <=> fibo2(3))
  fds.str(string(13,10))
  fds.dec(fibo2(3) <=> 10)
  fds.str(string(13,10))
  
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

PRI fibo1(i) : a | b, c
  b := 1
  c := 0
  repeat i
    c := a
    a := b + c
    b := c

#ifdef __SPIN2CPP__
''
'' traditional recursive definition
''
PRI fibo2(i)
  if (i < 2)
    return i
  return fibo2(i-1) + fibo2(i-2)

#else

'' Spin "optimized" version that relies on
'' stack behavior of the Spin compiler
'' our PASM implementation has to act like this
'' note that C++ will evaluate right to left rather
'' than left to right, so it won't work in C++ mode

PRI fibo2(i) : a | b
  b := 1
  repeat i
    a := b + (b := a)
#endif
