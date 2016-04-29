VAR
  long mask

PUB tx(x)
  repeat 4
    OUTA := x++
  

PUB str(s) | c
  REPEAT while ((c := byte[s++]) <> 0)
    tx(c)
  
