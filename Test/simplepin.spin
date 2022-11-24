CON
  numPins = 32
  DEFAULT_PIN = 4
  
VAR
  long pin

PUB tx(c)
  OUTA[pin] := c

PUB getpin()
  return DEFAULT_PIN
