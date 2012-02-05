PUB func
  return fooptr

DAT
bar
  long 1
  org  $+4
foo
  long 2
fooptr
  long foo
