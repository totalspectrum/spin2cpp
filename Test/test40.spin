PUB tx(character)
  outa := character

PUB dec(value) | i, x

'' Print a decimal number

  x := value == NEGX                                                            ' Check for max negative
  if value < 0
    value := ||(value+x)                                                        ' If negative, make positive; adjust for max negative
    tx("-")                                                                     ' and output sign

  i := 1_000_000_000                                                            ' Initialize divisor

  repeat 10                                                                     ' Loop for 10 digits
    if value => i                                                               
      tx(value / i + "0" + x*(i == 1))                                          ' If non-zero digit, output digit; adjust for max negative
      value //= i                                                               ' and digit from value
      result~~                                                                  ' flag non-zero found
    elseif result or i == 1
      tx("0")                                                                   ' If zero digit (or only digit) output it
    i /= 10                                                                     ' Update divisor
