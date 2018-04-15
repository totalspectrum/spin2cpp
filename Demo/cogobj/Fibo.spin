''
'' Fibonacci computer
''
PUB calc(n)
  if (n < 2)
    return n
  return calc(n-1) + calc(n-2)
  