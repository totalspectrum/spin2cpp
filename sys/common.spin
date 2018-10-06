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

dat
rx_method
tx_method
   long 0	'  objptr does not matter
   long 0	' special tag for _tx/_rx function

  '' 8 possible open files
  '' each one has 3 method pointers
  '' sendchar, recvchar, close
  ''
bas_handles
   long @@@tx_method, @@@rx_method, 0
   long 0[7*3]

pri _basic_open(h, sendf, recvf, closef)
  if (h +> 7)
    return
  h := h*3
  bas_handles[h] := sendf
  bas_handles[h+1] := recvf
  bas_handles[h+2] := closef

pri _basic_close(h) | ptr, closef, f, o
  if (h +> 7)
    return
  h := h*3
  bas_handles[h] := 0
  bas_handles[h+1] := 0
  closef := bas_handles[h+2]
  bas_handles[h+2] := 0
  if closef == 0
    return
  '' unpack the function pointer
  o := long[closef]
  f := long[closef+4]
  
  asm
    add sp, #4
    mov ptr, objptr
    mov objptr, o
    call f
    mov objptr, ptr
  endasm

pri _basic_print_char(h, c) | saveobj, t, f, o
  h := h*3
  t := bas_handles[h]
  if t == 0
    return
  o := long[t]
  f := long[t+4]
  if f == 0
    _tx(c)
    return
  asm
    add  sp, #8
    wrlong c, sp
    sub  sp, #8
    mov saveobj, objptr
    mov objptr, o
    call f
    mov objptr, saveobj
  endasm

pri _basic_print_string(h, ptr)|c
  repeat while ((c := byte[ptr++]) <> 0)
    _basic_print_char(h, c)

pri _basic_put(h, ptr, siz)|c
  repeat while (siz-- > 0)
    _basic_print_char(h, byte[ptr++])

pri _basic_digit(d)
  return (d < 10) ? d + "0" : (d-10) + "A"

pri _basic_fmt_instr(u, x, base, mindigits, maxdigits, fillchar) | digit, i
  if maxdigits == 1
    if x => base
      byte[u] := "*"
    else
      byte[u] := _basic_digit(x)
    return 1

  digit := x +// base
  x := x +/ base
  digit := _basic_digit(digit)
  if (x > 0) or (mindigits > 1)
    if mindigits > 1
      mindigits := mindigits - 1
      if x == 0 and digit == 0
        digit := fillchar
    i := _basic_fmt_instr(u, x, base, mindigits, maxdigits-1, fillchar)
  else
    i := 0
  byte[u + i] := digit
  return i+1
  
pri _basic_fmt_uinteger(x, base=10, mindigits=1, maxdigits=32, fillchar=$20) | u, r
  u := _gc_alloc_managed(maxdigits+1)
  if u == 0
    return u
  r := _basic_fmt_instr(u, x, base, mindigits, maxdigits, fillchar)
  byte[u+r] := 0
  return u
  
pri _basic_print_unsigned(h, x, base=10, mindigits=1, maxdigits=32, fillchar=$20) | ptr
  ptr := _basic_fmt_uinteger(x, base, mindigits, maxdigits, fillchar)
  _basic_print_string(h, ptr)
  _gc_free(ptr)
  
pri _basic_print_integer(h, x, base=10, mindigits=1, maxdigits=32, fillchar=$20, signchar=0)
  if (x < 0)
    _basic_print_char(h, "-")
    x := -x
'  elseif signchar > 0
'    _basic_print_char(h, "x")
  _basic_print_unsigned(h, x, base, mindigits, maxdigits, fillchar)
    
pri _basic_print_fixed(h, x) | i, f
  if (x < 0)
    _basic_print_char(h, "-")
    x := -x
  i := x >> 16
  f := x & $ffff
  ' now i is integer part, f is fractional
  ' round f off: 0.00005 ~= 3 in 16.16
  f := f + 2 
  if (f > $ffff)
    i++
    f -= $10000
  _basic_print_unsigned(h, i)
  _basic_print_char(h, ".")
  repeat 4
    f := f * 10
    i := f >> 16
    f := f & $ffff
    _basic_print_char(h, i + "0")
  return

pri _basic_print_nl(h)
  _basic_print_char(h, 13)
  _basic_print_char(h, 10)

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

pri _string_concat(x, y) | lenx, leny, ptr
  lenx := strsize(x)
  leny := strsize(y)
  ptr := _gc_alloc_managed(lenx + leny + 1)
  if ptr
    bytemove(ptr, x, lenx)
    bytemove(ptr + lenx, y, leny)
    byte[ptr + lenx + leny] := 0
  return ptr

pri chr`$(x) | ptr
  ptr := _gc_alloc_managed(2)
  if (ptr)
    byte[ptr+0] := x
    byte[ptr+1] := 0
  return ptr

pri left`$(x, n) | ptr, i, m
  if (n =< 0)
    return ""
  m := strsize(x)
  if (m =< n)
    return x
  ptr := _gc_alloc_managed(n+1)
  if ptr
    bytemove(ptr, x, n)
    byte[ptr+n] := 0
  return ptr
  
pri right`$(x, n) | ptr, i, m
  if (n =< 0)
    return ""
  m := strsize(x)
  if (m =< n)
    return x

  ptr := _gc_alloc_managed(n+1)
  if ptr
    i := m-n
    bytemove(ptr, x+i, n+1)
  return ptr

pri mid`$(x, i, j=9999999) | ptr, m, n
  if (j =< 0)
    return ""
  --i ' convert from 1 based to 0 based
  m := strsize(x)
  if (m < i)
    return ""
  ' calculate number of chars we will copy
  n := (m-i)
  n := (n > j) ? j : n
  ptr := _gc_alloc_managed(n+1)
  bytemove(ptr, x+i, n)
  byte[ptr+n] := 0
  return ptr
  
pri val(s) : r | c
  r := 0
  repeat
    c := byte[s++]
    if (c => "0") and (c =< "9")
      r := 10 * r + (c - "0")
    else
      return r
      
pri _make_methodptr(o, func) | ptr
  ptr := _gc_alloc_managed(8)
  if (ptr)
    long[ptr] := o
    long[ptr+4] := func
  return ptr

pri SendRecvDevice(sendf, recvf = 0, closef = 0)
  return (sendf, recvf, closef)

pri _basic_get_char(n) | t, o, f, saveobj
  n := n*3
  t := bas_handles[n+1]
  if t == 0
    return -1
  o := long[t]
  f := long[t+4]
  if f == 0
    return _rx
  asm
    add sp, #4
    mov saveobj, objptr
    mov objptr, o
    call f
    mov objptr, saveobj
    mov t, result1
  endasm
  return t

'' read n characters from handle h
pri input`$(n, h=0) | c, i, s
  s := _gc_alloc_managed(n+1)
  if s == 0
    return s
  repeat i from 0 to n-1 
    repeat
      c := _basic_get_char(h)
    until c => 0
    byte[s+i] := c
  byte[s+n] := 0
  return s

'' pause for m milliseconds
pri pausems(m)
  waitcnt(getcnt + m * (clkfreq / 1000))
