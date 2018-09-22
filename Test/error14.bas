sub fillstring1(s as ubyte pointer)
  s(1) = 0
  s = "whatever" ' discarding const attribute
end sub

sub fillstring2(s as const ubyte pointer)
  s(1) = 0      ' assignment to const object
  s = "goodbye"
end sub

sub fillstring3(s as const ubyte const pointer)
  s(1) = 0     ' assignment to const object
  s = "hello"  ' assignment to const object
end sub

function sum(x, y)
  if (x=y) goto here
  return x+y
end function

here:
  print "out of functions"
