PUB copy1(a, b, n) | i
  repeat i from 0 to n-1
    byte[a++] := byte[b++]

PUB copy2(a, b, n)
  repeat n
    byte[a++] := byte[b++]

    