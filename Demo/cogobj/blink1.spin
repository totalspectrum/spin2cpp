''
'' blink an LED in another COG
''
CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000
  pausetime = 25_000_000
  pin = 15

VAR
  long BlinkStack[32]	' stack space for Blink cog

OBJ
  fds: "FullDuplexSerial"

PUB demo | id, count
  fds.start(31, 30, 0, 115200)
  count := 0
  id := cognew(Blink(@count), @BlinkStack)
  fds.str(string("blink running in cog "))
  fds.dec(id)
  fds.tx(13)
  fds.tx(10)
  repeat
    waitcnt(CNT+80_000_000)
    fds.str(string("blinked "))
    fds.dec(count)
    fds.str(string(" times", 13, 10))

PUB Blink(countptr)
  DIRA[pin] := 1
  repeat
    OUTA[pin] ^= 1
    waitcnt(CNT+pausetime)
    long[countptr]++
  
