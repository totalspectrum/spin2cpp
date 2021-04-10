''
'' common code for both P1 and P2
''
con
  _rxtx_echo = 1
  _rxtx_crnl = 2
  
dat
__rxtxflags
    long _rxtx_echo | _rxtx_crnl


pri longfill(ptr, val, count)
  repeat count
    long[ptr] := val
    ptr += 4
pri wordfill(ptr, val, count)
  repeat count
    word[ptr] := val
    ptr += 2
pri longmove(dst, src, count) : origdst
  origdst := dst
  if dst < src
    repeat count
      long[dst] := long[src]
      dst += 4
      src += 4
  else
    dst += 4*count
    src += 4*count
    repeat count
      dst -= 4
      src -= 4
      long[dst] := long[src]
      
pri wordmove(dst, src, count) : origdst
  origdst := dst
  if dst < src
    repeat count
      word[dst] := word[src]
      dst += 2
      src += 2
  else
    dst += 2*count
    src += 2*count
    repeat count
      dst -= 2
      src -= 2
      word[dst] := word[src]
  
pri __builtin_strlen(str) : r=long
  r := 0
  repeat while byte[str] <> 0
    r++
    str++
pri __builtin_strcpy(dst, src) : r=@byte | c
  r := dst
  repeat
    c := byte[src++]
    byte[dst++] := c
  until c==0

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

{{
' obsolete, use _basic_print_float
pri _basic_print_fixed(h, x, fmt, ch) : r | i, f
  if (x < 0)
    r := _basic_print_char(h, "-")
    x := -x
  i := x >> 16
  f := x & $ffff
  ' now i is integer part, f is fractional
  ' round f off: 0.00005 ~= 3 in 16.16
  f := f + 2 
  if (f > $ffff)
    i++
    f -= $10000
  r += _basic_print_unsigned(h, i, fmt, 10)
  r += _basic_print_char(h, ".")
  repeat 4
    f := f * 10
    i := f >> 16
    f := f & $ffff
    r += _basic_print_char(h, i + "0")
  return r
}}

''
'' Spin2 FRAC operator: divide (n<<32) by d producing q
''
pri _qfrac(n, d) : q | r
  q,r := _div64(n, 0, d)
  
''
'' x*y >> 30, signed scaled multiply
''
pri _scas(x, y) : r | hi, lo
  lo := x * y
  hi := x ** y
  lo := lo >> 30
  hi := hi << 2
  return hi | lo
  
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

pri _funcptr_cmp(x, y) | xc, yc, d
  xc := long[x]
  yc := long[y]
  d := xc - yc
  if d == 0
    xc := long[x+4]
    yc := long[y+4]
    d := xc - yc
  return d

pri _string_cmp(x, y) : rval = long | xc, yc, d
  repeat
    xc := byte[x++]
    yc := byte[y++]
    rval := xc - yc
  while (rval==0) and xc and yc

pri _string_concat(x, y) : ptr = @byte | lenx, leny
  lenx := __builtin_strlen(x)
  leny := __builtin_strlen(y)
  ptr := _gc_alloc_managed(lenx + leny + 1)
  if ptr
    bytemove(ptr, x, lenx)
    bytemove(ptr + lenx, y, leny+1)
  return ptr

pri _make_methodptr(o, func) | ptr
  ptr := _gc_alloc_managed(8)
  if (ptr)
    long[ptr] := o
    long[ptr+4] := func
  return ptr

pri SendRecvDevice(sendf, recvf = 0, closef = 0)
  return (sendf, recvf, closef)

'' read n characters from handle h
pri input`$(n=long, h=0) | c, i, s
  s := _gc_alloc_managed(n+1)
  if s == 0
    return s
  repeat i from 0 to n-1 
    c := _basic_get_char(h)
    if c < 0
      quit
    byte[s+i] := c
  byte[s+i] := 0
  return s

pri _getrxtxflags()
  return __rxtxflags

pri _setrxtxflags(f)
  __rxtxflags := f
  
pri _tx(c)
  if (c == 10) and (__rxtxflags & _rxtx_crnl)
    _txraw(13)
  _txraw(c)

pri _rx : r
  repeat
    r := _rxraw()
  until r <> -1
  if (r == 13) and ((__rxtxflags & _rxtx_crnl) <> 0)
    r := 10
  if (__rxtxflags & _rxtx_echo)
    if r == 127
      _tx(8) ' backspace
    else
      _tx(r)

pri __sendstring(func, str) | c
  repeat
    c := byte[str++]
    if c
      func(c)
  until c == 0

pri __builtin_clkfreq : r=+long
  return __clkfreq_var

pri __builtin_clkmode : r=+long
  return __clkmode_var
  
pri __builtin_inf : r=float
  return $7f800000

pri __builtin_nan(p) : r=float
  return $7fc00000

pri _lockmem(addr) | oldlock, oldmem
  '''_tx("L")
  '''_gc_errhex(addr)
  repeat
    repeat
      oldlock := _lockset(__lockreg)
    while oldlock
    oldmem := byte[addr]
    '''_tx("g")
    '''_gc_errhex(oldmem)
    if oldmem == 0
      byte[addr] := 1
    _lockclr(__lockreg)
  while oldmem <> 0

pri _unlockmem(addr) | oldlock
  '''_tx("u")
  '''_gc_errhex(addr)
  byte[addr] := 0
    
pri _pinwrite(pingrp, val) | mask, basepin, reg
  basepin := pingrp & $1f
  reg := pingrp & $20
  mask := (pingrp >> 6)
  mask := (1<<(mask+1)) - 1
  mask := mask << basepin
  val := (val << basepin) & mask
  if reg
    dirb |= mask
    outb := (outb & !mask) | val 
  else
    dira |= mask
    outa := (outa & !mask) | val

pri _pinread(pingrp) : y
  y := (pingrp & $20) ? inb : ina
  y >>= pingrp   ' implicitly relies on only bottom 5 bits being used
  pingrp >>= 6   ' now pingrp has number of pins to use
  y := y +| pingrp  ' y ZEROX pingrp in Spin2
  
'' wait for a COG to finish
pri _cogwait(id)
  repeat while _cogchk(id) <> 0

'' read a line of data from handle h
pri file "libsys/readdata.spin" _basic_read_line(h=0)

'' find the end of a string, or the next comma
pri file "libsys/readdata.spin" _basic_find_terminator(ptr)

'' read a string from another string
'' returns two values: the new string, and a pointer into
'' the original string right after where we stopped reading

pri file "libsys/readdata.spin" _basic_get_string(src)

'' read an integer from a string
'' returns (value,  new_string_pointer)

pri file "libsys/readdata.spin" _basic_get_integer(src = "") : r=long, ptr

'' read a float from a string
pri file "libsys/readdata.spin" _basic_get_float(src = "") : r=float, ptr

'' pause for m seconds
pri _waitsec(m=long) | freq
  freq := __clkfreq_var
  repeat while m > 0
    _waitx(freq)
    m--
    
'' pause for m milliseconds
pri _waitms(m=long) | freq
  freq := __clkfreq_var
  repeat while m > 1000
    _waitx(freq)
    m -= 1000
  if m > 0
    _waitx(m * (freq / 1000))

'' pause for m microseconds
pri _waitus(m=long) | freq
  freq := __clkfreq_var
  repeat while m > 1000000
    _waitx(freq)
    m -= 1000000
  if m > 0
    _waitx(m * (freq / 1000000))

' check to see if cnt > x
pri _pollct(x) : flag
  flag := x - _getcnt()
  asm
    sar flag, #31
  endasm
  'return (flag < 0) ? -1 : 0
  
'' alias for _fltl
pri _pinf(p)
  _fltl(p)
  
'' get some random bits 0-$FFFFFF
pri file "libsys/random.c" _randbits : r=long

'' get a random float x with 0.0 <= x < 1.0
pri file "libsys/random.c" _randfloat : r=float

'' basic RND function
pri file "libsys/random.c" _basic_rnd(x=long) : r=float

'' low level file system functions
pri file "libc/unix/vfs.c" _getrootvfs()
pri file "libc/unix/vfs.c" _setrootvfs(root)
pri file "libc/unix/mount.c" _mount(name, volume)
pri file "libc/unix/mount.c" __getfilebuffer() : r=@byte
pri file "libc/unix/mount.c" __getvfsforfile(name, orig_name)
pri file "libc/unix/exec.c" _execve(name=string, argv=0, envp=0)

'' I/O functions
pri file "libsys/fmt.c" _basic_open(h, sendf, recf, closef)
pri file "libsys/fmt.c" _basic_open_string(h, str, iomode)
pri file "libsys/fmt.c" _basic_close(h)
pri file "libsys/fmt.c" _basic_print_nl(h)
pri file "libsys/fmt.c" _basic_print_char(h, c, fmt = 0)
pri file "libsys/fmt.c" _basic_print_string(h, ptr, fmt = 0)
pri file "libsys/fmt.c" _basic_print_integer(h, x, fmt = 0, base=10)
pri file "libsys/fmt.c" _basic_print_unsigned(h, x, fmt = 0, base=10)
pri file "libsys/fmt.c" _basic_print_longinteger(h, xlo, xhi, fmt = 0, base=10)
pri file "libsys/fmt.c" _basic_print_longunsigned(h, xlo, xhi, fmt = 0, base=10)
pri file "libsys/fmt.c" _basic_get_char(h)
pri file "libsys/fmt.c" _basic_put(h=long, pos=+long, ptr, elemnts=+long, size=+long)
pri file "libsys/fmt.c" _basic_get(h=long, pos=+long, ptr, elements=+long, size=+long)

'' format functions
pri file "libsys/fmt.c" _fmtchar(fn, fmt, c)
pri file "libsys/fmt.c" _fmtstr(fn, fmt, str)
pri file "libsys/fmt.c" _fmtnum(fn, fmt, x, base)
pri file "libsys/fmt.c" _fmtfloat(fn, fmt, x, spec)

'' string functions
pri file "libsys/strings.bas" left`$(x=string, n) : r=string
pri file "libsys/strings.bas" right`$(x=string, n): r=string
pri file "libsys/strings.bas" mid`$(s=string, n=0, m=9999999): r=string
pri file "libsys/strings.bas" chr`$(x): r=string
pri file "libsys/strings.bas" str`$(x=float): r=string
pri file "libsys/strings.bas" strint`$(x=long): r=string
pri file "libsys/strings.bas" bin`$(x, n=0): r=string
pri file "libsys/strings.bas" decuns`$(x, n=0): r=string
pri file "libsys/strings.bas" hex`$(x, n=0): r=string
pri file "libsys/strings.bas" oct`$(x, n=0): r=string

pri file "libsys/strings.bas" countstr(x=string,s=string): r=long
pri file "libsys/strings.bas" delete`$(t=string,o,n): r=string
pri file "libsys/strings.bas" insert`$(x=string,y=string,p=0): r=string
pri file "libsys/strings.bas" lcase`$(x=string): r=string
pri file "libsys/strings.bas" lpad`$(x=string, w, ch=string): r=string
pri file "libsys/strings.bas" ltrim`$(s=string): r=string
pri file "libsys/strings.bas" removechar`$(x=string, c=string): r=string
pri file "libsys/strings.bas" replacechar`$(x=string, c=string, n=string): r=string
pri file "libsys/strings.bas" reverse`$(x=string): r=string
pri file "libsys/strings.bas" rpad`$(x=string, w, ch=string): r=string
pri file "libsys/strings.bas" rtrim`$(s=string): r=string
pri file "libsys/strings.bas" space`$(n): r=string
pri file "libsys/strings.bas" string`$(n, x): r=string
pri file "libsys/strings.bas" trim`$(s=string): r=string
pri file "libsys/strings.bas" ucase`$(x=string): r=string
pri file "libsys/strings.bas" instr(off, x, y): r=long
pri file "libsys/strings.bas" instrrev(off, x, y): r=long

pri file "libc/stdlib/errno.c" _seterror(r)
pri file "libc/stdlib/errno.c" _geterror(): r=long
pri file "libc/stdlib/errno.c" _geterrnoptr(): r=@long

pri file "libc/string/strerror.c" _strerror(e=long): r=string

pri file "libsys/c_startup.c" _c_startup()

' compare unsigned alo, ahi, return -1, 0, or +1
pri _int64_cmpu(alo, ahi, blo, bhi) : r
  asm
      cmp  alo, blo wc,wz
      cmpx ahi, bhi wc,wz
 if_a mov  r, #1
 if_b neg  r, #1
  endasm
  
' compare signed alo, ahi, return -1, 0, or +1
pri _int64_cmps(alo, ahi, blo, bhi) : r
  asm
      cmp  alo, blo wc,wz
      cmpsx ahi, bhi wc,wz
 if_a mov  r, #1
 if_b neg  r, #1
  endasm

pri _int64_signx(x = long) : rlo, rhi
  rlo := x
  rhi := x
  asm
    sar rhi, #31
  endasm

pri _int64_zerox(x = long) : rlo, rhi
  rlo := x
  rhi := 0

pri _int64_add(alo, ahi, blo, bhi) : rlo, rhi
  rlo := alo
  rhi := ahi
  asm
    add  rlo, blo wc
    addx rhi, bhi
  endasm

pri _int64_sub(alo, ahi, blo, bhi) : rlo, rhi
  rlo := alo
  rhi := ahi
  asm
    sub  rlo, blo wc
    subx rhi, bhi
  endasm

pri _int64_and(alo, ahi, blo, bhi) : rlo, rhi
  rlo := alo
  rhi := ahi
  asm
    and  rlo, blo
    and rhi, bhi
  endasm

pri _int64_or(alo, ahi, blo, bhi) : rlo, rhi
  rlo := alo
  rhi := ahi
  asm
    or  rlo, blo
    or  rhi, bhi
  endasm

pri _int64_xor(alo, ahi, blo, bhi) : rlo, rhi
  rlo := alo
  rhi := ahi
  asm
    xor  rlo, blo
    xor  rhi, bhi
  endasm

pri _int64_shl(alo, ahi, count, counthi) : rlo, rhi | tmp
  rlo := alo
  rhi := ahi
  count &= 63
  if count > 32
    rhi := rlo
    rlo := 0
    count -= 32
  rhi := rhi << count
  tmp := rlo >> (32-count)
  rhi |= tmp
  rlo := rlo << count

pri _int64_shr(alo, ahi, count, counthi) : rlo, rhi | tmp
  rlo := alo
  rhi := ahi
  count &= 63
  if count > 32
    rlo := rhi
    rhi := 0
    count -= 32
  rlo := rlo >> count
  tmp := rhi << (32-count)
  rlo |= tmp
  rhi := rhi >> count

pri _int64_sar(alo, ahi, count, counthi) : rlo, rhi | tmp
  rlo := alo
  rhi := ahi
  count &= 63
  if count > 32
    rlo := rhi
    rhi := 0
    count -= 32
  rlo := rlo ~> count
  tmp := rhi << (32-count)
  rlo |= tmp
  rhi := rhi ~> count

pri _int64_muls(alo, ahi, blo, bhi) : rlo, rhi
  rlo := alo * blo
  rhi := alo ** blo
  rhi += ahi * blo
  rhi += bhi * alo

pri _int64_divmodu(nlo, nhi, dlo, dhi) : qlo, qhi, rlo, rhi |ilo, ihi
  rlo := nlo
  rhi := nhi
  ilo := dlo
  ihi := dhi
  qlo := 0
  qhi := 0
  if (dlo == 0 and dhi == 0)
    qlo := qhi := $ffff_ffff
    return
  repeat while (_int64_cmpu(nlo, nhi, ilo, ihi) > 0) and (ihi>>31) == 0
    ilo, ihi := _int64_add(ilo, ihi, ilo, ihi)
  repeat while (_int64_cmpu(ilo, ihi, dlo, dhi) > 0)
    qlo, qhi := _int64_add(qlo, qhi, qlo, qhi)
    if _int64_cmpu(rlo, rhi, ilo, ihi) >= 0
      rlo, rhi := _int64_sub(rlo, rhi, ilo, ihi)
      qlo |= 1
      ilo, ihi := _int64_shr(ilo, ihi, 1, 0)

pri _int64_divu(nlo, nhi, dlo, dhi) : qlo, qhi | x0, x1
  qlo, qhi, x0, x1 := _int64_divmodu(nlo, nhi, dlo, dhi)

pri _int64_modu(nlo, nhi, dlo, dhi) : rlo, rhi | x0, x1
  x0, x1, rlo, rhi := _int64_divmodu(nlo, nhi, dlo, dhi)
