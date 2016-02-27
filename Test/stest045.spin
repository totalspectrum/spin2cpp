PUB strlen2(x) | r
  r := 0
  repeat while (byte[x] <> 0)
    x++
    r++
  return r
