VAR
  long c
  byte b
  
PUB byteextend1(x)
  ~~long[x++]
  return x

PUB byteextend2
  ~c

PUB byteextend3
  c := ~b
