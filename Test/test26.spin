VAR
  LONG thelock
  LONG x

PUB lock
  repeat while lockset(thelock) <> 0
  x := 0
  repeat while (x < 4)
    lockret(x)
    x++




