CON
  pin = 15
  pausetime = 25_000_000
  
PUB run(countptr)
  DIRA[pin] := 1
  repeat
    OUTA[pin] ^= 1
    waitcnt(CNT+pausetime)
    long[countptr]++

