const heapsize = 2048

class counter
  dim n as integer
  sub set(x as integer)
    n = x
  end sub
  sub incr()
    n = n+1
  end sub
  function value() as integer
    return n
  end function
end class

dim as counter a

a.set(1)

print "Initial counter value="; a.value()

incrbyref(a)
print "Outside sub counter value="; a.value()

incrbyval(a)
print "Outside sub counter value="; a.value()


sub incrbyref(byref x as counter)
  x.incr()
  print "inside sub: counter="; x.value()
end sub

sub incrbyval(byval y as counter)
  y.incr()
  print "inside sub: counter="; y.value()
end sub

''
'' tests of multiple assignment
''

dim as integer p, q, r
dim as string b$

sub doprint(x as integer, y as string, z as integer)
  print x; ": "; y; " "; z
end sub

function multival1(x=0 as integer) as integer,string
  return (x+1), ("mval" + str$(x))
end function

function multival2() as string, integer
  return "uval2", 99
end function

print

p,b$ = multival1(90)

print "p="; p; " b$="; b$

doprint(multival1(2), -2)
doprint(multival1(), -3)
doprint(3, multival2())

function incr(x, n=1)
  return x + n
end function
print incr(2, 2)
print incr(2)

'
' test of READ/DATA
'
testread
testread
testread

' some basic 64 bit stuff
test64bits

' and exit
doexit 0

sub testread
  dim x as uinteger
  read x
  print x; "=$"; hex$(x)
end sub

data &h00_ff_fe, 0b1011, 100

''
'' send a special exit status for propeller-load
'' FF 00 xx, where xx is the exit status
''

sub doexit(status)
  print \255; \0; \status;
  ' just loop here so that quickstart board does not
  ' let the pins float
  do
  loop
end sub

''
'' test for 64 bit arithmetic and printing
''
sub test64bits()
  dim as longint x, y
  x = 123456780012
  x = (x/100) + 90
  y = (x * 1000) - 1
  print
  print "64 bits: x="; x; " y="; y
end sub
