''
'' common code for both P1 and P2
''

pri longfill(ptr, val, count)
  repeat count
    long[ptr] := val
    ptr += 4
pri longmove(dst, src, count)
  repeat count
    long[dst] := long[src]
    dst += 4
    src += 4
pri wordfill(ptr, val, count)
  repeat count
    word[ptr] := val
    ptr += 2
pri wordmove(dst, src, count)
  repeat count
    word[dst] := word[src]
    dst += 2
    src += 2
pri bytefill(ptr, val, count)
  repeat count
    byte[ptr] := val
    ptr += 1
pri bytemove(dst, src, count)
  repeat count
    byte[dst] := byte[src]
    dst += 1
    src += 1
pri strsize(str) : r
  r := 0
  repeat while byte[str] <> 0
    r++
    str++
pri strcomp(s1, s2) | c1, c2
  repeat
    c1 := byte[s1++]
    c2 := byte[s2++]
    if (c1 <> c2)
      return 0
  until (c1 == 0)
  return -1
pri _lookup(x, b, arr, n) | i
  i := x - b
  if (i => 0 and i < n)
    return long[arr][i]
  return 0
pri _lookdown(x, b, arr, n) | i
  repeat i from 0 to n-1
    if (long[arr] == x)
      return i+b
    arr += 4
  return 0
pri _lfsr_forward(x) | a
  if (x == 0)
    x := 1
  a := $8000000b
  repeat 32
    asm
      test x, a wc
      rcl  x, #1
    endasm
  return x
pri _lfsr_backward(x) | a
  if (x == 0)
    x := 1
  a := $17
  repeat 32
    asm
      test x, a wc
      rcr  x, #1
    endasm
  return x

pri _basic_print_char(c)
  return _tx(c)
    
pri _basic_print_string(ptr)|c
  repeat while ((c := byte[ptr++]) <> 0)
    _basic_print_char(c)

pri _basic_put(ptr, siz)|c
  repeat while (siz-- > 0)
    _basic_print_char(byte[ptr++])
    
pri _basic_print_unsigned(x, base=10) | d
  d := x +// base  ' unsigned modulus
  x := x +/ base  ' unsigned divide
  if (x)
    _basic_print_unsigned(x, base)
  if (d > 9)
    d := (d - 10) + ("a" - "0") 
  _basic_print_char(d + "0")
    
pri _basic_print_integer(x)
  if (x < 0)
    _basic_print_char("-")
    x := -x
  _basic_print_unsigned(x)
    
pri _basic_print_fixed(x) | i, f
  if (x < 0)
    _basic_print_char("-")
    x := -x
  i := x >> 16
  f := x & $ffff
  ' now i is integer part, f is fractional
  ' round f off: 0.00005 ~= 3 in 16.16
  f := f + 2 
  if (f > $ffff)
    i++
    f -= $10000
  _basic_print_unsigned(i)
  _basic_print_char(".")
  repeat 4
    f := f * 10
    i := f >> 16
    f := f & $ffff
    _basic_print_char(i + "0")
  return

pri _basic_print_nl
  _basic_print_char(13)
  _basic_print_char(10)

''
'' fixed point multiply
''
pri _fixed_mul(x, y) | hi, lo
  lo := x * y
  hi := x ** y
  return (hi << 16) | (lo >> 16)

''
'' for divide, we want (x / y) << N, but
'' we have to do it in parts, because
'' probably x<<N will overflow
''
pri _fixed_div(x, y, n) | sign
  sign := (x^y) >> 31
  x := ||x
  y := ||y
  ' figure out how far left we can shift x
  repeat while (x > 0 and n > 0)
    x <<= 1
    --n
  y := y
  x := (x +/ y)
  if (sign)
    x := -x
  return x << n

pri _string_cmp(x, y) | xc, yc, d
  repeat
    xc := byte[x++]
    yc := byte[y++]
    d := xc - yc
  while (d==0) and xc and yc
  return d
