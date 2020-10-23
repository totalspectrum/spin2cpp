pub demo
  info(shl64(i64($1234, $5678), i64(0, 4)))

PUB i64(a, b): x,y
  x := a
  y := b

PUB shl64(ahi, a, bhi, b) : chi, c | t
  chi := ahi << b
  t := a >> (32-b)
  chi |= t
  c := a << b

PUB info(a, b)
  outa := a
  outb := b
  