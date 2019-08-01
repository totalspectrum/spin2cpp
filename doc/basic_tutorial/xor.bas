' declare which pin we will use
const LED=16
' set it as an output
direction(LED) = output

' declare a subroutine to toggle a pin
sub toggle(pin)
  output(pin) = output(pin) xor 1
end sub

' loop forever
do
  toggle(LED)
  pausems(1000)
loop
