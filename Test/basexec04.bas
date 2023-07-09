' default most things to float
defsng a-z
' except for i-k
defint i-k

let a = 1
let i = 2

print a, i

'' closure test for BASIC
const HEAPSIZE = 8000

dim printer as sub(t as integer)
dim stepper as sub()

sub constructFuncs(n as integer, msg as string)
  dim count as integer
  count = 1

  printer = sub(t as integer)
    print msg; count
  end sub
  stepper = sub()
    count = count + n
  end sub
end sub

constructFuncs(1, "step1: ")
for i = 1 to 5
  printer(i)
  stepper
next
constructFuncs(2, "try2: ")
for i = 1 to 5
  printer(i)
  stepper
next
print "done funcs"

'' now test throw/catch
dim sa as class using "throwtest.bas"
dim errnum as ulong
dim err as string
try
  print sa.setval(4)
  print sa.incval(-2)
  print sa.incval(-2)
  print sa.incval(-2)
catch errnum
  err = cast(any, errnum)
  print "caught error "; err
end try
print "done try; val = "; sa.getval()

' define a type of function integer -> integer
type IntFunc as function(x as integer) as integer

' given a function f, return a new function that does
' f(f(f(...f(x)))) n times (f^n(x))

function funcpow(n as integer, f as IntFunc) as IntFunc
  return [x%: var r=x%: var i=n: while i > 0: r=f(r): i=i-1: wend:=>r]
end function

' a slow way to do multiplication
function slowtimes(a as integer, b as integer) as integer
  dim as IntFunc f
  f = funcpow(a, [x%:=>x%+b])
  return f(0)
end function

' now some test code
dim j as integer

for i = 0 to 10
  j = 12-i
  print i, j, slowtimes(i, j)
next i

'
' test for interfaces and classes
'

interface floatable
  implements function asFloat() as single
end interface

class wrapFloat
  dim f as single
  function asFloat() as single
    return f
  end function
  sub set(a as single)
    f = a
  end sub
end class
class wrapInt
  dim f as integer
  sub set(a as integer)
    f = a
  end sub
  function asFloat() as single
    return f
  end function
end class

sub testit(x as floatable)
  print x.asFloat() mod 2.1
end sub

dim as wrapInt x3
dim as wrapFloat y0, y1, y2

x3.set(3)
y0.set(3.1)
y1.set(0.5)
y2.set(-2.1)

testit(x3)
testit(y0)
testit(y1)
testit(y2)

''
'' send the magic propload status code
''
print \255; \0; \0;
