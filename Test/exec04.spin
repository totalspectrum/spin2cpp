CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
  ser: "FullDuplexSerial"
  fds = "FullDuplexSerial"
  
PUB demo | x,y,n

  ser.start(31, 30, 0, 115200)

  ser.str(string("lookup/lookdown tests", 13, 10))

  testup(0, 9, 4)
  testup(1, 9, 4)
  testup(4, 9, 4)
  testup(5, 9, 4)

  testdown(@ser, 4)
  testdown(@ser, 7)
  testdown(@ser, 3)
  testdown(@ser, 99)

  exit

PUB testup(i, x, y) | r,oldi
  r := lookup(i : x, y, 3, 4, 5)
  ser.str(string("lookup( "))
  ser.dec(i)
  ser.str(string(": "))
  ser.dec(x)
  ser.str(string(", "))
  ser.dec(y)
  ser.str(string(", 3, 4, 5) = "))
  ser.dec(r)
  newline
  r := lookupz(i : x, y, 3, 4, 5)
  ser.str(string("lookupz( "))
  ser.dec(i)
  ser.str(string(": "))
  ser.dec(x)
  ser.str(string(", "))
  ser.dec(y)
  ser.str(string(", 3, 4, 5) = "))
  ser.dec(r)
  newline
  oldi := i
  r := lookup(i++ : i, i+1, 2, 3, 4)
  ser.str(string("lookup( "))
  ser.dec(oldi)
  ser.str(string(": "))
  ser.dec(i)
  ser.str(string(", "))
  ser.dec(i+1)
  ser.str(string(", 2, 3, 4) = "))
  ser.dec(r)
  newline

PUB testdown(term, i) | r,oldi
  r := lookdown(i : 8, 7, 3, 4, 5)
  fds[term].str(string("lookdown( "))
  fds[term].dec(i)
  fds[term].str(string(": "))
  fds[term].str(string("8, 7, 3, 4, 5) = "))
  fds[term].dec(r)
  newline
  r := lookdownz(i : 1, 2, 3, 4, 5)
  fds[term].str(string("lookdownz( "))
  fds[term].dec(i)
  fds[term].str(string(": "))
  fds[term].str(string("1, 2, 3, 4, 5) = "))
  fds[term].dec(r)
  newline
  oldi := i
  r := lookdown(i^=1 : i, 2, 3, 4, 5)
  fds[term].str(string("lookdownz( "))
  fds[term].dec(oldi)
  fds[term].str(string(": "))
  fds[term].dec(i)
  fds[term].str(string(", 2, 99, 98, 5) = "))
  fds[term].dec(r)
  newline

PUB newline
  ser.str(string(13, 10))

PUB exit
'' send an exit sequence which propeller-load recognizes:
'' FF 00 xx, where xx is the exit status
''
  ser.tx($ff)
  ser.tx($00)
  ser.tx($00) '' the exit status
  ser.txflush
  repeat

