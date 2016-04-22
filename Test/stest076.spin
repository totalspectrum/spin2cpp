''
'' shift a byte x out a pint
''
pub shiftout(x, pin)
  repeat 8
    outa[pin] := (x&1)
    x := x >> 1
