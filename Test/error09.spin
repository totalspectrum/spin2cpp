VAR
  long a,b,c

PUB bug1
  (a,b) := c

PUB bug2
  a := (b,c)

PUB bug3
  (a, b) := (a,b,c)

