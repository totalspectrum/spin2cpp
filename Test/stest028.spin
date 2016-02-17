PUB divmod16(x, y) | q,r
  q := x / y
  r := x // y
  return (q<<16) | r
