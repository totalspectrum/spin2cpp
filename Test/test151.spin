VAR
  long a
  long b
  
'' this function expects a string
PUB countspaces(a = string("")) | n, c
  n := 0
  repeat
    c := byte[a++]
    if (c == 0)
      quit
    if (c == " ")
      n++
  return n

'' this function just expects an integer
PUB addup(x)
  a += x

PUB demo
  a := countspaces("hello, world")
  b := countspaces
  addup("x")
