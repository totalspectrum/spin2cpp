sub putx(c as ubyte)
  if c >= asc("a") and c <= asc("z") then
    c = c + (asc("A") - asc("a"))
  end if
  print \c;
end sub

sub closex
  print "closed handle"
end sub

open SendRecvDevice(@putx, nil, @closex) as #2
print "hello, world!"
print #2, "hello, world!"
print #2
print #2, "good"; "bye!"
close #2

print #2, "this should be ignored"

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

''
'' send the magic propload status code
''
print \255; \0; \0;
