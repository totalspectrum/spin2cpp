CON
  _clkfreq = 80_000_000
  A = 2
  B = 3
  
PUB main() : y | x
  x := 3
  y := 0
  org
    mov outa, #1
    if A == B
      mov outa, #2
      mov y, #1
    else
      mov outa, #0
      mov y, #2
    end
    mov outb, #1
  end
  return y
