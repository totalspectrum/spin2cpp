CON
  numPins = 32

VAR
  long mypin

PUB start(pin)
  mypin := pin
  
PUB tx(c)
  OUTA[mypin] := c
