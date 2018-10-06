pub myfill(ptr, v, count)
  repeat count
    long[ptr] := v
    ptr += 4

PUB fillzero(ptr, count)
  myfill(ptr, 0, count)

PUB fillone(ptr, count)
  myfill(ptr, -1, count)

