VAR
  long pin

PUB tx(c)
  OUTA[pin] := c
  