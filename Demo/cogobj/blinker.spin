CON
  pin = 2
  
PUB run(countptr)
  repeat
    OUTA[pin] := 1
    OUTA[pin] := 0
    long[countptr]++
