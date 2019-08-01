' declare which pin we will use
const LED=16
' set it as an output
direction(LED) = output

' declare a subroutine to toggle a pin
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
