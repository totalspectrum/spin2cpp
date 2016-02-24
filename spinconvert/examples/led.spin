CON
  _clkmode = xtal1+pll16x
  _clkfreq = 80_000_000
  pin = 15

PUB start | pause
  pause := (_clkfreq/2)

  DIRA[pin] := 1  ' set as output
  OUTA[pin] := 1  ' turn it on
  repeat
      waitcnt(CNT + pause)
      !OUTA[pin]
