PUB checkit | i, c
  i := 0
  repeat
    case c := INA & $FF
      "y", "Y":
        quit
      other:
        i++

  return i*27
