' xor arrays a with array b
' n is the length of the arrays
PUB xorbytes(a, b, n)
  repeat n
    byte[a++] ^= byte[b++]
    