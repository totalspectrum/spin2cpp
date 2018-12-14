CON
  numPins = 32
  
VAR
  long pin

PUB tx(c)
  OUTA[pin] := c
