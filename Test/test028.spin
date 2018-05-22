VAR
  LONG thelock
  LONG x

PUB lock
  x := 0
  repeat until (x > 9)
    lockret(x)
    x++

  x := 0
  repeat
    lockret(x)
    x++
  while x < 10




