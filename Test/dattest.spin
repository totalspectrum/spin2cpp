#include "stdconsts.spinh"

PUB demo | n, i, b

  '' start up the serial port
  fds.start(31, 30, 0, 115200)

  n := getvar
  i := getvar2
  b := getvar3

  fds.str(string("n="))
  fds.hex(n, 8)
  fds.str(string(13,10))
  fds.str(string("i="))
  fds.hex(i, 8)
  fds.str(string(13,10))
  fds.str(string("b="))
  fds.hex(b, 8)
  fds.str(string(13,10))
  exit

PUB exit
  waitcnt(cnt + 40000000)
#ifndef __P2__  
  fds.stop
#endif

PUB getvar
  return myvar

PUB getvar2
  return mybyte

PUB getvar3
  return myby2

PUB setvar(x)
  myvar := x
 
DAT

myvar long $11223344
mybyte
myby2	byte $ab
	byte $cd
	byte $11
	byte $22

