VAR
  long val
  
PUB setval(n)
  if (n == 0)
    abort -99
  val := n
  return n

PUB incval(n)
  setval(val + n)
  return 0
