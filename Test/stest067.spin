PUB fibo(x)
  if (x < 2)
    return x
  return fibo(x-1) + fibo(x-2)
