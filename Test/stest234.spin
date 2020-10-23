PUB demo(x, y) | a[2]
  'longmove(@a, @x, 2)
  a[0] := x
  a[1] := y
  asm
    add  x, a+0 wc
    addx y, a+1
  endasm
  return y
  