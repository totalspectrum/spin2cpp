rem simple program to toggle a pin

const lopin = 16

'' number of cycles in a millisecond
const mscycles = 80_000

direction(lopin) = output
output(lopin) = 1
do
  pausems 500
  output(lopin) = not output(lopin)
loop

sub pausems ms as uinteger
  dim tim as uinteger
  tim = ms * mscycles
  waitcnt(tim + getcnt())
end sub
