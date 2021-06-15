CON
#ifdef __P2__
  _clkfreq = 160_000_000
  rx_pin = 63
  tx_pin = 62
  default_baud = 230_400
#else  
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  rx_pin = 31
  tx_pin = 30
  default_baud = 115_200
#endif  
  twof = 2.0
  roottwof = ^^twof

OBJ
#ifdef __P2__
  fds: "spin/SmartSerial"
#else  
  fds: "spin/FullDuplexSerial"
#endif  
  sa: "setabort.spin"
  
PUB demo | x,y,z,i,r

  fds.start(rx_pin, tx_pin, 0, default_baud)

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
#ifdef __SPIN2PASM__
  z := (x:=i++) + (y:=i++)
#else
  ' C++ handles  assignment inside expressions differently than Spin
  x := i++
  y := i++
  z := x+y
#endif
  fds.str(string("expr eval: x="))
  fds.hex(x,4)
  fds.str(string(" y="))
  fds.hex(y,4)
  fds.str(string(" z="))
  fds.hex(z,4)
  newline

  i := 100
#ifdef __SPIN2PASM__
  info(++i, ++i)
#else
  info(i+1, i+2)
#endif
  fds.str(string("multiple parameter test", 13, 10))
  info(shl64(i64($1234, $56789ABC), i64(0, 4)))
  info(add64(i64(0, $ffffffff), 0, 3))
  fds.str(string("abort test", 13, 10))
  r := satest1
  if r
    fds.str(string("aborted", 13, 10))
  fds.str(string("abort test2", 13, 10))
  r := \satest2
  if r
    fds.str(string("aborted", 13, 10))
  else
    fds.str(string("done", 13, 10))

  exit

PUB satest1
  result := \sa.setval(2)
  fds.dec(result)
  newline
  result := \sa.incval(-2)
  fds.dec(result)
  newline
  result := 0

PUB satest2
  result := sa.setval(3)
  fds.dec(result)
  newline
  result := sa.incval(-3)
  fds.dec(result)
  newline
  result := 0

PUB i64(a, b): x,y
  x := a
  y := b

PUB shl64(ahi, a, bhi, b) : chi, c | t
  chi := ahi << b
  t := a >> (32-b)
  chi |= t
  c := a << b

#ifdef __OUTPUT_BYTECODE__
PUB add64(ahi, a, bhi, b) : chi, c
  chi := ahi + bhi
  c := a + b
'  if c +< a
'    chi++
#else
PUB add64(ahi, a, bhi, b) : chi, c
  chi := ahi
  c := a
  asm
    add c, b wc
    addx chi, bhi
  endasm
#endif

PUB info(x,y)
  fds.str(string("func eval: x="))
  fds.hex(x,8)
  fds.str(string(" y="))
  fds.hex(y,8)
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
