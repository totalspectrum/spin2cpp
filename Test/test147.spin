PUB dbl64(ahi, alo): bhi, blo
  bhi := ahi + ahi
  blo := alo + alo
  if (alo & $80000000)
    bhi++

' function to quadruple a 64 bit number
PUB quad64(ahi, alo)
  return dbl64(dbl64(ahi, alo))
