' declare which pin we will use
#ifdef __propeller2__
const LED=56
#else
const LED=16
#endif

' set it as an output
direction(LED) = output

' declare a subroutine to toggle a pin
' using an if statement like this is a bit
' cumbersome
sub toggle(pin)
  if output(pin) = 0 then
    output(pin) = 1
  else
    output(pin) = 0
  end if
end sub

' loop forever
do
  toggle(LED)
  pausems(1000)
loop
