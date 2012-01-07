PUB fun(X,Y)
  case X
    $00: 
        case Y
          0: !outa[0]
          1: !outa[1]
    20: !outa[2]
    OTHER:
      !outa[3]
