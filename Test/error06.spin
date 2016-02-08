''
'' check for parameter matching
''
PUB main
  foo(2,3)

PRI foo(x)
  OUTA := bar(x, 1)

PRI bar(x, y, z)
  return x + y + z
