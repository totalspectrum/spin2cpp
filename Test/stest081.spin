var
  long x, y, z, w
  
pub test(a, b, c)
   x := (a|b)
   y := (a|b)|c
   a += 1
   z := (a|b)
   w := (a|b)|c
 
