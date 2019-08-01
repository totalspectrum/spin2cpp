const LED = 16

sub forever( f as sub() )
  f()
  forever(f)
end sub

direction(LED) = output

forever( [: output(LED) = output(LED) xor 1 : pausems 100 :] )
