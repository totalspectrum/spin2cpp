
VAR
  long clk, di
  
pub send(outv) | c, d
  outv ><= 8

  d := di
  repeat 8
    outb[d] := outv
    outv >>= 1
  outb[d] := 1
