CON
  _ = 1
  __ = 2

DAT
myarray byte 1
  byte long 2, 3
dummy1
  byte 4
someval
  long @dummy1 - @myarray
  
PUB foo
  return _ + myarray[1]
