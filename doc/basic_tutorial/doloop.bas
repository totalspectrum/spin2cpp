' declare which pin we will use
const LED=16
' set it as an output
direction(LED) = output

' loop forever
do
  output(LED) = 1 ' set the pin high
  pausems(1000)    ' wait 1 second
  output(LED) = 0 ' set the pin low
  pausems(1000)    ' wait another second
loop
