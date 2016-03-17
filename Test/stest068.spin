PUB shiftout(x, pin)
  repeat 32
    OUTA[pin] := x
    x >>= 1
