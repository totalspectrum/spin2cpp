
PUB foo
  return someaddr

DAT
  org 0
entry
  nop
someaddr
  long $, $+1
