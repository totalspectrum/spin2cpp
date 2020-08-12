#include "stdconsts.spinh"

VAR
  long x
  
PUB demo | a, b

  '' start up the serial port
  fds.start(rxpin, txpin, 0, baud)

  a := getvar1
  fds.str(string("a="))
  fds.hex(a, 8)
  fds.str(string(13,10))
  b := getvar2
  fds.str(string("b="))
  fds.hex(b, 8)
  fds.str(string(13,10))

  x.long[0] := $11223344
  x.byte[1] := $aa
  fds.str(string("x="))
  fds.hex(x, 8)
  fds.str(string(13,10))

  exit

PUB getvar1
  return myvar1

PUB getvar2
  return myvar2

PUB exit
'' send an exit sequence which propeller-load recognizes:
'' FF 00 xx, where xx is the exit status
''
  fds.tx($ff)
  fds.tx($00)
  fds.tx($00) '' the exit status
  fds.txflush
  repeat

DAT

	byte $1
myvar1
myvar2	long $11223344


