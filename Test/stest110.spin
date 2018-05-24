PUB filllong(a, b, n)
  asm
loop
     wrlong b, a
     add a, #4
     djnz n, #loop
  endasm