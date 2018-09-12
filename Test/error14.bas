sub fillstring1(s as ubyte pointer)
  s(1) = 0
  s = "whatever"
end sub

sub fillstring2(s as const ubyte pointer)
  s(1) = 0      ' error
  s = "goodbye"
end sub

sub fillstring3(s as ubyte const pointer)
  s(1) = 0
  s = "hello"  ' error
end sub

