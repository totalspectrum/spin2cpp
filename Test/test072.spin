PUB top(x)
  return \sub1(x)

PRI sub1(x)
  return 2*sub2(x)

PRI sub2(x)
  if (x == 3)
    ' weird case
    abort 99
  return x+1
