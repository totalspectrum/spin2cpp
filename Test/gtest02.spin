PUB run | i,now,elapsed,x

  repeat i from 1 to 15
     now := cnt
     x := fibo(i)
     elapsed := cnt - now

PUB fibo(x)
  if (x < 2)
    return x
  return fibo(x-1) + fibo(x-2)
