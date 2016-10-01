pub proc1(v, n) | p, i, y
  p := n-1
  y := 0
  repeat while (p > 0)
    y += long[v][p]
    p--
  long[v][0] := y + (n - 1)
  