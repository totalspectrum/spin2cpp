VAR
  long a
  long r
  long stack[32]

PUB demo
  blah
  
PUB cfunc(x)
  repeat
    OUTA := x++

PUB blah
  r := 1+cognew(cfunc(1), @stack)

PUB getfunc(x)
  return INA | x
