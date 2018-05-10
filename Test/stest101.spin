VAR
  long a
  
PUB checkcmd | cmdreg
  cmdreg := 1
  repeat
    repeat
    while cmdreg<>0
    a++
    cmdreg := 0
 

PUB cmd2(reg)
  repeat
    OUTA := reg
    reg := 0
    