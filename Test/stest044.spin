PUB strlen(x) | r, c
  r := -1
  repeat
    c := byte[x]
    x++
    r++
  while c <> 0
  return r
