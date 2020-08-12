#include "stdconsts.spinh"

OBJ
#ifdef __P2__
  fdsobj = "spin/SmartSerial"
#else
  fdsobj = "spin/FullDuplexSerial"
#endif

PUB demo | x,y,n

  fds.start(31, 30, 0, 115200)

  fds.str(string("lookup/lookdown tests", 13, 10))

  testup(0, 9, 4)
  testup(1, 9, 4)
  testup(4, 9, 4)
  testup(5, 9, 4)

  testdown(@fds, 4)
  testdown(@fds, 7)
  testdown(@fds, 3)
  testdown(@fds, 99)

  exit

PUB testup(i, x, y) | r,oldi
  r := lookup(i : x, y, 3, 4, 5)
  fds.str(string("lookup( "))
  fds.dec(i)
  fds.str(string(": "))
  fds.dec(x)
  fds.str(string(", "))
  fds.dec(y)
  fds.str(string(", 3, 4, 5) = "))
  fds.dec(r)
  newline
  r := lookupz(i : x, y, 3, 4, 5)
  fds.str(string("lookupz( "))
  fds.dec(i)
  fds.str(string(": "))
  fds.dec(x)
  fds.str(string(", "))
  fds.dec(y)
  fds.str(string(", 3, 4, 5) = "))
  fds.dec(r)
  newline
  oldi := i
  r := lookup(i++ : i, i+1, 2, 3, 4)
  fds.str(string("lookup( "))
  fds.dec(oldi)
  fds.str(string(": "))
  fds.dec(i)
  fds.str(string(", "))
  fds.dec(i+1)
  fds.str(string(", 2, 3, 4) = "))
  fds.dec(r)
  newline

PUB testdown(term, i) | r,oldi
  r := lookdown(i : 8, 7, 3, 4, 5)
  fdsobj[term].str(string("lookdown( "))
  fdsobj[term].dec(i)
  fdsobj[term].str(string(": "))
  fdsobj[term].str(string("8, 7, 3, 4, 5) = "))
  fdsobj[term].dec(r)
  newline
  r := lookdownz(i : 1, 2, 3, 4, 5)
  fdsobj[term].str(string("lookdownz( "))
  fdsobj[term].dec(i)
  fdsobj[term].str(string(": "))
  fdsobj[term].str(string("1, 2, 3, 4, 5) = "))
  fdsobj[term].dec(r)
  newline
  oldi := i
  r := lookdown(i^=1 : i, 2, 3, 4, 5)
  fdsobj[term].str(string("lookdownz( "))
  fdsobj[term].dec(oldi)
  fdsobj[term].str(string(": "))
  fdsobj[term].dec(i)
  fdsobj[term].str(string(", 2, 99, 98, 5) = "))
  fdsobj[term].dec(r)
  newline

PUB newline
  fds.str(string(13, 10))

PUB exit
'' send an exit sequence which propeller-load recognizes:
'' FF 00 xx, where xx is the exit status
''
  fds.tx($ff)
  fds.tx($00)
  fds.tx($00) '' the exit status
  fds.txflush
  repeat

