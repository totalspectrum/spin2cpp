VAR
  long SqStack[6]	' Stack space for Square cog
  
PUB demo | x,y

  x := 2
  cognew(Square(@X), @SqStack)

PUB Square(XAddr)
 ' Square the value at XAddr
 repeat
   long[XAddr] *= long[XAddr]
   waitcnt(80_000_000 + cnt)
   
