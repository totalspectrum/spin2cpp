CON

  cols = 40
  rows = 12

  screensize = cols * rows
  lastrow = screensize - cols

OBJ
  a: "test01.spin"

PUB getx
  return a#x
