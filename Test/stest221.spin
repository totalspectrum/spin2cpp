CON
  _clkfreq = 160_000_000
  
PUB demo
  if (0)
    blah(1)
  else
    barg(2)

PRI blah(x) | i
  repeat i from 1 to x
    outb := i
    outb := string("hello")

PRI barg(y) | i
  repeat i from y to 1
    outb := i
    outb := string("goodbye")
