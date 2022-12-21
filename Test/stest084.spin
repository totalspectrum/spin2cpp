var
  long x
  long y
  
pub update(a, b)
  x := a
  y := b
  
pub bump
  update(x+y,x+y)
  update(x+y,x+y)
