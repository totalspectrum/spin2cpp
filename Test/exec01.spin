#include "stdconsts.spinh"

VAR
  long m
  
PUB demo | x,y

  fds.start(rxpin, txpin, 0, baud)

  fds.str(string("Forward test", 13, 10))

  x := 1
  putnum(x?)
  putnum(x?)
  putnum(?x)
  putnum(?x)

  fds.str(string("Backward test", 13, 10))
  x := 1
  putnum(?x)
  putnum(?x)

  ' test default parameters
  check
  check(1)
  check(1,2)

  ' check operator precedence
  m := $ff
  m := m + 1
  if not m & $00ff
    fds.str(string("overflow correctly detected", 13, 10))
  else
    fds.str(string("operator precedence problem", 13, 10))  
  exit

PUB check(x = $a, y = $b)
  fds.str(string("check called x="))
  fds.hex(x,4)
  fds.str(string(" y="))
  fds.hex(y,4)
  fds.tx(13)
  fds.tx(10)

PUB exit
'' send an exit sequence which propeller-load recognizes:
'' FF 00 xx, where xx is the exit status
''
  fds.tx($ff)
  fds.tx($00)
  fds.tx($00) '' the exit status
  fds.txflush
  repeat
  
PUB putnum(x)
  fds.dec(x)
  fds.str(string(13, 10))
