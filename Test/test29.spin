VAR
  LONG strlock
  LONG idx

PUB tx(val)
  byte[$7000][idx] := 0

PUB str(stringptr)

'' Send string                    

  repeat while lockset(strlock)
  repeat strsize(stringptr)
    tx(byte[stringptr++])
  lockclr(strlock)
