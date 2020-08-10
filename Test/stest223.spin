CON
  _clkfreq = 160_000_000
  mychoice = 3
  
PUB {++opt(remove-dead)} demo
  case mychoice
    1:      blah(1)
    2:      barg(2)
    other:  blah(3)

PRI blah(x) | i
  repeat i from 1 to x
    outb := i
    outb := string("hello")

PRI barg(y) | i
  repeat i from y to 1
    outb := i
    outb := string("goodbye")
