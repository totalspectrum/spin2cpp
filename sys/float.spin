
''***************************************
''*  Floating-Point Math                *
''*  Single-precision IEEE-754          *
''*  Author: Chip Gracey                *
''*  Copyright (c) 2006 Parallax, Inc.  *
''*  Modified by: Eric Smith            *
''*  Copyright (c) 2018 Total Spectrum Software Inc.
''*  See end of file for terms of use.  *
''***************************************
con
  _float_one = $2000_0000
  
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
'' for a or b NaN, should return $8000000
'' generally we return a-b

CON
  f_infinity = $7f800000
  f_unordered = $80000000
pri _float_cmp(a=float, b=float) : r=long
  if (a < 0)
    if (b < 0)
      return a - b
    if b == 0 and a == $80000000
      return 0
    return -1
  if (b < 0)
    if a == 0 and b == $80000000
      return 0
    return 1
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

pri file "libsys/fmt.c" _basic_print_float(h, f, fmtparam)

' convert string to integer
pri __builtin_atoi(s = "0", base=0) : r | c, negate, digit
  negate := 0
  repeat while byte[s] == " "
    s++
  c := byte[s]
  if c == "-"
    s++
    negate := 1
    
  repeat
    c := byte[s++]
    if (c == "0") or (c == "&")
      digit := 0
    elseif (c => "1") and (c =< "9")
      digit := (c - "0")
      if (base == 0)
        base := 10
    elseif (base > 10) and (c => "a") and (c =< "f")
      digit := 10 + (c-"a")
    elseif (base > 10) and (c => "A") and (c =< "F")
      digit := 10 + (c-"A")
    else
      quit
    r := base * r + digit
  if (base == 0)
    if (c == "x" or c == "h")
      r := __builtin_atoi(s, 16)
    elseif c == "b"
      r := __builtin_atoi(s, 2)
  if negate
    r := -r

' convert string to float
pri __builtin_atof(s = "0") : r=float | c, exp, scaleexp, sawpoint, negate
  r := 0
  scaleexp := 0
  exp := 0
  negate := 0
  sawpoint := 0
  repeat while byte[s] == " "
    s++
  c := byte[s]
  if c == "+"
    s++
  elseif c == "-"
    s++
    negate := 1
  repeat
    c := byte[s++]
    if (c == 0)
      quit
    if c => "0" and c =< "9"
      r := _float_mul(r, 10.0)
      r := _float_add(r, _float_fromuns( c - "0" ))
      if sawpoint
        scaleexp -= 1
    elseif c == "."
      if sawpoint
        quit
      sawpoint := 1
    else
      quit
  if c == "E" or c == "e"
    ' need an exponent
    exp := __builtin_atoi(s) + scaleexp
  else
    exp := scaleexp
    
  exp := _float_pow_n(10.0, exp)
  r := _float_mul(r, exp)
  if negate
    r := _float_negate(r)

'' extract sign from a float
pri __builtin_signbit(a=float) : r=long
  r := a >> 31

'' copy the sign from y to x; result has the magnitude of x, but
'' sign of y
pri __builtin_copysign(x=float, y=float) : r=float
  x := (x<<1) >> 1
  r := (y>>31) << 31
  r |= x

pri __builtin_ilogb(a=float) : r=long | s, x, m
  (s,x,m) := _float_Unpack(a)
  if m == 0
    return -$7fffffff
  if x == 128
    if m == _float_one
      return $7fffffff
    return $8000_0000  ' NaN
  return x
  
  
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