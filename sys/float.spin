
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
  _float_sign_bit = $8000_0000
  
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
    negate := _float_sign_bit
  else
    negate := 0
  single := _float_fromuns(integer) ^ negate
   
pri _float_fromuns64(lo, hi) : m = float | s, x

''Convert integer to float
  if hi == 0
    m := lo
    if m == 0
      return m
    x := >|m - 1                'get exponent
    m <<= 31 - x                'msb-justify mantissa
  else
    m := hi
    x := >|m - 1
    s := 31 - x
    if s
      m <<= s
      lo >>= (-s)
      m |= lo
    x += 32

  s := 0                      'get sign
  m >>= 2                     'bit29-justify mantissa
  m := _float_Pack(s, x, m)
  return m
    
pri _float_fromint64(lo, hi) : single = float | negate, tmplo, tmphi

''Convert integer to float    
  if hi < 0
    tmplo := lo
    tmphi := hi
    lo := 0
    hi := 0
    asm
      sub lo, tmplo wc
      subx hi, tmphi
    endasm
    negate := _float_sign_bit
  else
    negate := 0
  single := _float_fromuns64(lo, hi) ^ negate

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
  


pri file "libsys/ieee32.c" _float_add(singleA = float, singleB = float) : single = float

''Subtract singleB from singleA
pri _float_sub(singleA = float, singleB = float) : single = float
  return _float_add(singleA, singleB ^ _float_sign_bit)

             
pri file "libsys/ieee32.c" _float_sqrt(singleA = float) : single = float
pri file "libsys/ieee32.c" _float_mul(singleA=float, singleB=float) : single=float
pri file "libsys/ieee32.c" _float_div(singleA=float, singleB=float) : single=float

''
'' compare a and b;
'' return 0 if a = b, -N if a < b, +N if a > b
'' for a or b NaN, should return the value u
'' generally we return a-b

con
  f_infinity = $7f800000
  f_unordered = $80000000

pri _float_cmp(a=float, b=float, u=1) : r=long
  if (a > $7f800000) or (b > $7f800000)
    return u
  if (a +> $ff800000) or (a +> $ff800000)
    return u
  if (a < 0)
    if (b < 0)
      return b - a
    if b == 0 and a == $80000000
      return 0
    return -1
  if (b < 0)
    if a == 0 and b == $80000000
      return 0
    return 1
  return a - b
  
pri {++noinline}  _float_tointeger(a=float, r) : integer=long | s, x, m

'Convert float to rounded/truncated integer

  (s,x,m) := _float_Unpack(a)
  if x > 30
    m := (s) ? $8000_0000 : $7FFF_FFFF
  elseif x < -1
    m := 0
  else
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
    midf := _float_pow_n(1.0, 10.0, mid)
    t := _float_div(x, midf)
    if (_float_cmp(t, 10.0) => 0)
      lo := mid
    elseif (_float_cmp(t, 1.0) < 0)
      hi := mid
    else
      return (midf, mid)
  return (_float_pow_n(1.0, 10.0, hi), hi)

pri file "libsys/fmt.c" _basic_print_float(h, f, fmtparam, ch)

' convert string to integer
pri __builtin_atoi(s = "0", base=0) : r = long | c, negate, digit
  negate := digit := 0
  repeat while byte[s] == " "
    s++
  c := byte[s]
  repeat while (c == "-") or (c == "+")
    s++
    if (c == "-")
      negate := 1 - negate
    c := byte[s]

  if base == 0
    if c == "$"
      base := 16
      s++
    elseif c == "%"
      base := 2
      s++

  repeat
    repeat
      c := byte[s++]
    while c == "_"
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
    if (c == "x" or c == "h" or c == "X" or c == "H")
      r := __builtin_atoi(s, 16)
    elseif c == "b" or c == "B"
      r := __builtin_atoi(s, 2)
    elseif c == "o" or c == "O"
      r := __builtin_atoi(s, 8)
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
  repeat while (c == "+") or (c == "-")
    if c == "-"
      negate ^= _float_sign_bit
    s++
    c := byte[s]
    
  repeat
    repeat
      c := byte[s++]
    while c == "_"
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
    
  r := _float_pow_n(r, 10.0, exp) ^ negate

'' extract sign from a float
pri __builtin_signbit(a=float) : r=long
  r := a >> 31

'' copy the sign from y to x; result has the magnitude of x, but
'' sign of y
pri __builtin_copysign(x=float, y=float) : r=float
  x := (x<<1) >> 1
  r := (y>>31) << 31
  r |= x

' calculate the log base 2 of a float
pri __builtin_ilogb(a=float) : r=long | s, x, m
  (s,x,m) := _float_Unpack(a)
  if m == 0
    return -$7fffffff
  if x == 128
    if m == _float_one
      return $7fffffff
    return $8000_0000  ' NaN
  return x

' determine whether a float is NaN or not
pri __builtin_isnanf(a=float) : r | s, x, m
  (s,x,m) := _float_Unpack(a)
  if x < 128 %orelse m == _float_one
    return false
  return true

' calculate log2 of a fixed point number
pri __builtin_ilogb_fixed(a) : r=long | n
  n := >|a
  r := n - 17

' copy the sign of a fixed point number
pri __builtin_copysign_fixed(x, s) : r=long
  if s < 0
    if x < 0
      r := x
    else
      r := -x
  else
    if x < 0
      r := -x
    else
      r := x

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

pri file "libsys/s_floorf.c" __builtin_floorf(x = float) : r=float
pri file "libsys/s_ceilf.c" __builtin_ceilf(x = float) : r=float
pri file "libsys/math.c"  __builtin_sinf(x = float) : r=float
pri file "libsys/math.c"  __builtin_cosf(x = float) : r=float
pri file "libsys/math.c"  __builtin_tanf(x = float) : r=float
pri file "libsys/math.c"  __builtin_fabsf(x = float) : r=float
pri file "libsys/math.c"  __builtin_asinf(x = float) : r=float
pri file "libsys/math.c"  __builtin_acosf(x = float) : r=float
pri file "libsys/math.c"  __builtin_atanf(x = float) : r=float
pri file "libsys/math.c"  __builtin_atan2f(y = float, x = float) : r=float
pri file "libsys/s_frexpf.c" __builtin_frexpf(x = float, p=@long) : r=float
pri file "libsys/s_ldexpf.c" __builtin_ldexpf(x = float, n=long) : r=float
pri file "libsys/s_modf.c" __builtin_modff(x = float, p) : r=float
pri file "libsys/powers.c" __builtin_expf(x = float) : r=float
pri file "libsys/powers.c" __builtin_exp2f(x = float) : r=float
pri file "libsys/powers.c" __builtin_exp10f(x = float) : r=float
pri file "libsys/powers.c" __builtin_logf(x = float) : r=float
pri file "libsys/powers.c" __builtin_log2f(x = float) : r=float
pri file "libsys/powers.c" __builtin_log10f(x = float) : r=float
pri file "libsys/powers.c" __builtin_powf(x = float, y = float) : r=float
pri file "libsys/powers.c" __builtin_logbase(x = float, y = float) : r=float
pri file "libsys/ieee32.c"   _float_pow_n(b=float, a=float, n=long) : r=float
