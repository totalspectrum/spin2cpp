VAR
  long arr[10]

OBJ
  led[10] : "pinobj"

VAR
  long i
  
PUB setptr2(c)
  led[nextidx].tx(c)

PUB nextidx
  return i++
