PUB prod(x,y)
  x *= y
  return x

PUB cube(x)
  return prod(x, prod(x,x))
