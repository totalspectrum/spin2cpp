'' closure test for BASIC
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
print "done"

''
'' send the magic propload status code
''
print \255; \0; \0;
