
''***************************************
''*  Floating-Point Math                *
''*  Single-precision IEEE-754          *
''*  Author: Chip Gracey                *
''*  Copyright (c) 2006 Parallax, Inc.  *
''*  Modified by: Eric Smith            *
''*  Copyright (c) 2018 Total Spectrum Software Inc.
''*  See end of file for terms of use.  *
''***************************************

pri _float_fromuns(integer = long) : m = float | s, x

''Convert integer to float    
  m := integer
  if (m == 0)
    return m

  s := 0                      'get sign
  x := >|m - 1                'get exponent
  m <<= 31 - x                'msb-justify mantissa
  m >>= 2                     'bit29-justify mantissa
  m := _float_Pack(s, x, m)
  return m
    
pri _float_fromint(integer = long) : single = float | negate

''Convert integer to float    
  if integer < 0
    integer := -integer
    negate := 1
  else
    negate := 0
  single := _float_fromuns(integer)
  if (negate)  
    single := _float_negate(single)
   
pri _float_round(single = float) : integer = long

''Convert float to rounded integer

  return _float_tointeger(single, 1)    'use 1/2 to round


pri _float_trunc(single = float) : integer = long

''Convert float to truncated integer

  return _float_tointeger(single, 0)    'use 0 to round


pri _float_negate(singleA = float) : single = float

''Negate singleA

  return singleA ^ $8000_0000   'toggle sign bit
  
pri _float_abs(singleA = float) : single = float

''Absolute singleA

  return singleA & $7FFF_FFFF   'clear sign bit
  

pri _float_sqrt(singleA = float) : single = float | s, x, m, root

''Compute square root of singleA

  if singleA > 0                'if a =< 0, result 0

    (s,x,m) := _float_Unpack(singleA)
    m >>= !x & 1                'if exponent even, shift mantissa down
    x ~>= 1                     'get root exponent

    root := $4000_0000          'compute square root of mantissa
    repeat 31
      result |= root
      if result ** result > m
        result ^= root
      root >>= 1
    m := result >> 1
  
    return _float_Pack(s, x, m)             'pack result


pri _float_add(singleA = float, singleB = float) : single = float | sa, xa, ma, sb, xb, mb

''Add singleA and singleB

  (sa,xa,ma) := _float_Unpack(singleA)          'unpack inputs
  (sb,xb,mb) := _float_Unpack(singleB)

  if sa                         'handle mantissa negation
    ma := -ma
  if sb
    mb := -mb

  result := ||(xa - xb) <# 31   'get exponent difference
  if xa > xb                    'shift lower-exponent mantissa down
    mb ~>= result
  else
    ma ~>= result
    xa := xb

  ma += mb                      'add mantissas
  sa := ma < 0                  'get sign
  ||ma                          'absolutize result

  return _float_Pack(sa, xa, ma)              'pack result


pri _float_sub(singleA = float, singleB = float) : single = float

''Subtract singleB from singleA

  return _float_add(singleA, _float_negate(singleB))

             
pri _float_mul(singleA=float, singleB=float) : single=float | sa, xa, ma, sb, xb, mb

''Multiply singleA by singleB

  (sa,xa,ma) := _float_Unpack(singleA)          'unpack inputs
  (sb,xb,mb) := _float_Unpack(singleB)

  sa ^= sb                      'xor signs
  xa += xb                      'add exponents
  ma := (ma ** mb) << 3         'multiply mantissas and justify

  return _float_Pack(sa, xa, ma)              'pack result


pri _float_div(singleA=float, singleB=float) : single=float | sa, xa, ma, sb, xb, mb

''Divide singleA by singleB

  (sa,xa,ma) := _float_Unpack(singleA)          'unpack inputs
  (sb,xb,mb) := _float_Unpack(singleB)

  sa ^= sb                      'xor signs
  xa -= xb                      'subtract exponents

  repeat 30                     'divide mantissas
    result <<= 1
    if ma => mb
      ma -= mb
      result++        
    ma <<= 1
  ma := result

  return _float_Pack(sa, xa, ma)              'pack result

''
'' compute a^n, where a is a float and n is an integer
'' keeps as many bits of precision as possible
''
pri _float_pow_n(a=float, n=long) : r=float | sgnflag, invflag
  if (n < 0)
    invflag := 1
    n := -n
  else
    invflag := 0
  if (a < 0)
    sgnflag := n & 1
  else
    sgnflag := 0
  r := 1.0
  repeat while (n > 0)
    if (n & 1)
      r := _float_mul(r, a)
    n := n >> 1
    a := _float_mul(a, a)
  if (invflag)
    r := _float_div(1.0, r)
  if (sgnflag)
    r := -r

''
'' compare a and b;
'' return 0 if a = b, -N if a < b, +N if a > b
''
pri _float_cmp(a=float, b=float) : r=long
  if (a < 0)
    if (b < 0)
      return a - b
    if (b == 0 and a == $8000000)
      return 0
    return -1
  if (b < 0)
    if (a == 0 and b == $8000000)
      return 0
    '' otherwise a >= 0, so is bigger than b
    return 1
  '' in this case both are positive
  return a - b

pri _float_tointeger(a=float, r) : integer=long | s, x, m

'Convert float to rounded/truncated integer

  (s,x,m) := _float_Unpack(a)
  if x => -1 and x =< 30        'if exponent not -1..30, result 0
    m <<= 2                     'msb-justify mantissa
    m >>= 30 - x                'shift down to 1/2-lsb
    m += r                      'round (1) or truncate (0)
    m >>= 1                     'shift down to lsb
    if s                        'handle negation
      -m
    return m                    'return integer


pri _float_Unpack(single=float) : s, x, m | tmp

'_float_Unpack floating-point into (sign, exponent, mantissa) at pointer
  tmp := 0
  s := single >> 31             'unpack sign
  x := single << 1 >> 24        'unpack exponent
  m := single & $007F_FFFF      'unpack mantissa

  if x                          'if exponent > 0,
    m := m << 6 | $2000_0000    '..bit29-justify mantissa with leading 1
  else
    tmp := >|m - 23          'else, determine first 1 in mantissa
    x := tmp                 '..adjust exponent
    m <<= 7 - tmp            '..bit29-justify mantissa

  x -= 127                      'unbias exponent
  
pri _float_Pack(s, x, m) : single

'_float_Pack floating-point from (sign, exponent, mantissa) at pointer

  if m                          'if mantissa 0, result 0
  
    result := 33 - >|m          'determine magnitude of mantissa
    m <<= result                'msb-justify mantissa without leading 1
    x += 3 - result             'adjust exponent

    m += $00000100              'round up mantissa by 1/2 lsb
    if not (m & $FFFFFF00)        'if rounding overflow,
      x++                       '..increment exponent
    
    x := x + 127 #> -23 <# 255  'bias and limit exponent

    if x < 1                    'if exponent < 1,
      m := $8000_0000 +  m >> 1 '..replace leading 1
      m >>= -x                  '..shift mantissa down by exponent
      x~                        '..exponent is now 0

    return s << 31 | x << 23 | m >> 9 'pack result

''
'' calculate biggest power of 10 that is < x (so 1.0 <= x / F < 10.0f)
'' special case: if x == 0 just return 1
''
pri _float_getpowten(x) | midf, lo, hi, mid, t, sanity
  if (x == 0)
    return (1.0, 0)
  lo := -38
  hi := 38
  sanity := 0
  repeat while lo < hi and (sanity++ < 100)
    mid := (lo + hi) / 2
    midf := _float_pow_n(10.0, mid)
    t := _float_div(x, midf)
    if (_float_cmp(t, 10.0) => 0)
      lo := mid
    elseif (_float_cmp(t, 1.0) < 0)
      hi := mid
    else
      return (midf, mid)
  return (_float_pow_n(10.0, hi), hi)

pri _basic_print_float(h, f, fmtparam=0) | numdigits, i, lastf, exp, u, maxu, needpoint, needexp, digit, numzeros
  needexp := 0
  numdigits := 5 '' later this should be a parameter  
  if (f < 0)
    _basic_print_char(h, "-")
    f := _float_negate(f)
  (lastf, exp) := _float_getpowten(f)
  f := _float_div(f, lastf)
  if (_float_cmp(f, 10.0) > 0)
    _basic_print_char(h, "i")
    _basic_print_char(h, "n")
    _basic_print_char(h, "f")
    return

  ''
  '' sanity checks
  '' 
  if (numdigits > 7)
    numdigits := 7
  elseif (numdigits < 1)
    numdigits := 1
  lastf := _float_pow_n(10.0, numdigits)
  f := _float_mul(f, lastf)
  maxu := _float_trunc(lastf)
  u := _float_round(f)

  if (exp < 0)
    numzeros := -exp
    needpoint := 1
    if numzeros > (numdigits - 2)
      needexp := 1
      --exp
    else
      repeat i from 0 to numzeros
        u := (u + 5) +/ 10
  else
    if (numdigits =< exp)
      needpoint := 1
      needexp := 1
    else
      needpoint := exp+1

  repeat while (u => maxu)
    u := (u + 5) +/ 10
    exp++

  repeat while numdigits > 0
    --numdigits
    u := u * 10
    digit := u +/ maxu
    u := u +// maxu
    if (needpoint == 0)
      _basic_print_char(h, ".")
    _basic_print_char(h, "0" + digit)
    --needpoint

  if (needexp)
    _basic_print_char(h, "E")
    if (exp < 0)
      _basic_print_char(h, "-")
      exp := -exp
    else
      _basic_print_char(h, "+")
      --exp
    u := exp +/ 10
    exp := exp +// 10
    _basic_print_char(h, u + "0")
    _basic_print_char(h, exp + "0")

{{

┌──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                                   TERMS OF USE: MIT License                                                  │                                                            
├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation    │ 
│files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,    │
│modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software│
│is furnished to do so, subject to the following conditions:                                                                   │
│                                                                                                                              │
│The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.│
│                                                                                                                              │
│THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE          │
│WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR         │
│COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,   │
│ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                         │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
}}    