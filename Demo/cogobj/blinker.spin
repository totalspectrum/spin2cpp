'' blinker.spin
PUB run(pin, pausetime)
  DIRA[pin] := 1
  repeat
    OUTA[pin] ^= 1
    waitcnt(CNT+pausetime)

