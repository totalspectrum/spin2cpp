PUB Unpack_x(single)
  return dounpack_x(single, 0)

PUB Unpack_m(single)
  return dounpack_x(single, 1)
  
PRI dounpack_x(single, which) | x, m, tmp
'Unpack floating-point into (sign, exponent, mantissa) at pointer

  x := single << 1 >> 24        'unpack exponent
  m := single & $007F_FFFF      'unpack mantissa

  if x                          'if exponent > 0,
    m := m << 6 | $2000_0000    '..bit29-justify mantissa with leading 1
  else
    tmp := >|m - 23          'else, determine first 1 in mantissa
    x := tmp                 '..adjust exponent
    m <<= 7 - tmp            '..bit29-justify mantissa

  x -= 127                      'unbias exponent
  if (which)
    return x
  return m


