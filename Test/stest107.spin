PUB dbl64(ahi, alo): bhi, blo
  bhi := ahi
  blo := alo
  asm
    add blo, blo wc
    addx bhi, bhi
  endasm

' function to quadruple a 64 bit number
PUB quad64(ahi, alo)
  return dbl64(dbl64(ahi, alo))
