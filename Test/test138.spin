con
  ZERO = 0
  
dat

' offset 0
obst        byte $44
' offset 1
filler      byte ZERO[3]
' offset 4
cmd         word $5555

pub getobst
  return @obst
pub getfiller
  return @filler
pub getcmd
  return @cmd
