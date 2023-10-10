function for "cog" toupper(c as ubyte) as ubyte
  if c >= asc("a") and c <= asc("z") then
    c = c + (asc("A") - asc("a"))
  end if
  return c
end function

sub putx(c as ubyte)
  c = toupper(c)
  print \c;
end sub

sub closex
  print "closed handle"
end sub

open SendRecvDevice(@putx, nil, @closex) as #3
print "hello, world!"
print #3, "hello, world!"
print #3
print #3, "good"; "bye!"
close #3

print #3, "this should be ignored"

dim shared as integer x=-2, y=-3

/' this is a test '/

type myint as long
sub testint(x as myint)
  print using "+###:+%%%:-###:-%%%:###:%%%"; x, x, x, x, x, x
end sub
testint(0)
testint(-99)
testint(99)

var a$ = "abc"
print       " x   xx   xxxxxxx   xxxxxx   xxxxxxx"
print using "[!] [\\] [\<<<<<\] [\>>>>\] [\=====\]"; a$, a$, a$, a$, a$

print x; " "; y

for i# = 0.3 to 1.0001 step 0.1
  print i#
next i#

const two = 2
sub trybool(x as single)
  var a = two <> 2
  dim b as boolean

  b = x
  print "0 : a, b = ", a, b
  a -= 2
  b += 1
  print "1 : a, b = ", a, b
  a = two
  b += 1
  print "2 : a, b = ", a, b
  
end sub

trybool(1.0)


''
'' send the magic propload status code
''
print \255; \0; \0;
