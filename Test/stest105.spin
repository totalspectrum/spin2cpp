VAR
  long a, b, c

PUB swapab
  (a,b) := (b,a)

PUB seq3(n)
  return (n, n+1, n+2)

PUB setit1
  (a,b,c) := seq3(1)

PUB setit2
  a,b,c := seq3(0)
