''
'' Fibonacci computer
''
PUB fibo(n)
  if (n < 2)
    return n
  return fibo(n-1) + fibo(n-2)

''
'' asynchronous computation; useful if this
'' runs in another COG. We can start a fibo
'' calculation running, and then go away and
'' get the answer later
''
VAR
  long answer

'' make sure we do not explicitly return anything from startcalc
'' that way the calling Spin will not block

PUB startcalc(n)
  answer := fibo(n)

'' and a getter method to get the result
PUB calcresult
  return answer

