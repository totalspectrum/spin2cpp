PUB hexdigit(x)
  return lookupz( x & $F : "0".."9", "A".."F")
