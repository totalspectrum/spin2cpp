VAR
  byte buf[10]
  
PUB ptr(a = string("hello"))
  return a

PUB greet1
  return ptr

PUB greet2
  return ptr(string("hi"))

PUB fluff
  return @buf
