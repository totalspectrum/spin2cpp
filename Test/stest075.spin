pub mul(arg1, arg2) : r1
  r1 := 0

  repeat
    if (arg2 & 1)
      r1 += arg1
    arg2 >>= 1
    arg1 <<= 1
  while arg2 <> 0
  
