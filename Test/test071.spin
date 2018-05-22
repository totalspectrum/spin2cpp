PUB blah | i, x[8]
  repeat i from 0 to 7
    foo(@result, @x[i])

PUB foo(m,n)
  OUTA |= |< n
