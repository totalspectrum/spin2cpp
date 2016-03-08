pub myfill(ptr, val, count)
  repeat count
    long[ptr] := val
    ptr += 4

PUB fillzero(ptr, count)
  myfill(ptr, 0, count)

PUB fillone(ptr, count)
  myfill(ptr, -1, count)

