CON A=4
VAR
  long X

PUB fun(Y)
  case X+Y
    10,"C": !outa[0]
    A*2   : !outa[1]
    30..40: !outa[2]
            !outa[3]
    OTHER:  !outa[4]
  X += 5
