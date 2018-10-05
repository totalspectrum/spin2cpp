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

''
'' send the magic propload status code
''
print \255; \0; \0;
