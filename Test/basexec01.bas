rem
rem runtime tests for various BASIC features
rem

dim as single r, s
dim as integer i, j
dim as uinteger u

function rawbits(y as any)
  return y
end function

print "abs tests"
r = -5.2
print r, abs(r)

s = r + 3
print s, abs(s), abs(-s)

print "bit tests"

r = 7.9
print r, ">> 1 =", r>>1

r = &b1010
s = &b0011

print r; " and "; s, r and s, rawbits(r and s)

''
'' now test fixed point multiply and divide
''
const FACTOR = 10

let bot# = FACTOR
let scale# = 1.0 / bot#
sub doscale(x as integer)
  print "x, scaled", x, x * scale#, x / bot#, bot# / x
end sub

let n% = 1
for i = 1 to 6
  doscale(n%)
  n% = n% * 10
next i

'' printing values in a loop
dim x# as single
x# = 1.0
n% = 8
while x# > 0.0 and n% > 0
  x# = x# / 10
  print x#
  n% = n% - 1
wend

function checkasc(s as string)
  if asc(s) = asc("@") then
    return 1
  end if
  return 0
end function

print "asc: "; checkasc("@123"); ", "; checkasc("hello")

print "string tests:"

sub reportstr(a$, b$)
  print a$, b$, "< = >: ";
  print a$ < b$, a$ = b$, a$ > b$
end sub

reportstr("abc", "abc")
reportstr "abc", "a"
reportstr "def", "zzz"

''
'' send a special exit status for propeller-load
'' FF 00 xx, where xx is the exit status
''
dim xit(3) as ubyte

sub doexit(status)
  xit(1) = 255
  xit(2) = 0
  xit(3) = status
  put xit
  ' just loop here so that quickstart board does not
  ' let the pins float
  do
  loop
end sub

''
'' done
''
doexit(0)

