VAR
  LONG buf[20]
 
PUB func1
  byte[func2 + 11] |= $20

PUB func2
  return @buf

