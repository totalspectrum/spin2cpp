CON
  numPins = 32
  DEFAULT_PIN = 4
  
VAR
  long pin
  long zz
  
PUB tx(c)
  OUTA[pin] := c

PUB getpin()
  return DEFAULT_PIN

PUB getval(pin = DEFAULT_PIN)
  return INA[pin]
