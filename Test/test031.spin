CON
  A = 4

VAR
  long X

PUB fun(Y)
  case X+Y
    10    : !outa[0]
    A*2   : !outa[1]
            !outa[2]
    OTHER:  !outa[3]
  X += 5
