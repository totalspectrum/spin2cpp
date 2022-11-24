CON
  incval = 1
  
VAR
  long count

PUB get
  return count

PUB add(x)
  count := count + x
  return count

PUB inc
  return add(INCVAL)
