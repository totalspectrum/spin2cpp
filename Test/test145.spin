''
'' test for non-static methods called from
'' a function forced to be static
''
VAR
  long stack[4]
  long counter
  
PUB main
  incr(2)
  cognew(subfunc(1), @stack)

PUB incr(x)
  counter += x

PUB incr1
  counter += 1
  
PUB sfunc(x)
  OUTA := x
  
PUB subfunc(x)
  '' incr is non-static, so make sure we call via self->incr
  incr(x)
  '' same for incr1
  incr1
  '' sfunc is static, so no such worries
  '' OTOH it is safe to call it that way
  sfunc(x)