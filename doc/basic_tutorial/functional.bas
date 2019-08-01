const LED = 16

sub forever( f as sub() )
  f()
  forever(f)
end sub

sub toggle()
   output(LED) = output(LED) xor 1
   pausems 500
end sub

direction(LED) = output

forever( toggle )
