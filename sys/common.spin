''
'' common code for both P1 and P2
'' applies to both bytecode and PASM targets
''
con
  _rxtx_echo = 1
  _rxtx_crnl = 2
   
dat
__rxtxflags
    long _rxtx_echo | _rxtx_crnl
__clkfreq_ms
    long 0
__clkfreq_us
    long 0

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

pri __builtin_strncpy(dst, src, maxn) : r=@byte | n
  n := __builtin_strlen(src) <# maxn
  bytemove(dst, src, n)
  byte[dst][n] := 0
  return dst

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

pri _txhex(h) | c
  repeat 8
    h := (h<-4)
    c := h & $f
    c := (c < 10) ? c + "0" : (c-10) + "A"
    _txraw(c)
  _txraw(" ")

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

pri __sendstring(str) | c
  repeat
    c := byte[str++]
    if c
      __sendptr(c)
  until c == 0

pri __builtin_clkfreq : r=+long
  return __clkfreq_var

pri __builtin_clkmode : r=+long
  return __clkmode_var
  
pri __builtin_inf : r=float
  return $7f800000

pri __builtin_nan(p) : r=float
  return $7fc00000

pri __builtin_nanf(p) : r=float
  return $7fc00000

pri __builtin_bitreverse32(x = +long) : r = +long
  return x >< 32

pri {++specialfunc(pinw)} _pinwrite(pingrp, val) | mask, basepin, reg
  basepin := pingrp & $1f
  reg := pingrp & $20
  mask := (pingrp >> 6)
  mask := (1<<(mask+1)) - 1
  mask <<= basepin
  val <<= basepin
  if reg
    dirb |= mask
    outb := (outb & !mask) | (val & mask) 
  else
    dira |= mask
    outa := (outa & !mask) | (val & mask)
    
pri {++specialfunc(pinr)} _pinread(pingrp) : y
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
pri _waitms(m=long) | freq, freqms
  freq := __clkfreq_var
  repeat while m => 1000
    _waitx(freq)
    m -= 1000
  if m > 0
    m := (m * 1000) +/ freq
    _waitx(m)

'' pause for m microseconds
pri _waitus(m=long) | freq
  freq := __clkfreq_var
  repeat while m => 1000000
    _waitx(freq)
    m -= 1000000
  if m > 0
     m := (m * 1000000) +/ freq
    _waitx(m)


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
  
'' some math functions for C propeller2.h
pri _encod(x)
  if x == 0
    return 0
  return (>|x)-1

pri _clz(x) : r
  if x==0
    r := 32
  else
    r := 31 - _encod(x)

'' peek/poke
pri __peek(x = long) : r
  r := byte[x]
pri __wpeek(x = long) : r
  r := word[x]
pri __lpeek(x = long) : r
  r := long[x]

pri __poke(addr = long, val = long)
  byte[addr] := val
pri __wpoke(addr = long, val = long)
  word[addr] := val
pri __lpoke(addr = long, val = long)
  long[addr] := val

' @@@ means a reference to any type of variable
pri __addr(x = @@@)
  return x

'' get some random bits 0-$FFFFFF
pri file "libsys/random.c" _randbits : r=long

'' get a random float x with 0.0 <= x < 1.0
pri file "libsys/random.c" _randfloat : r=float

'' basic RND function
pri file "libsys/random.c" _basic_rnd(x=long) : r=float

'' low level file system functions
'' marked as "complexio" because if used we need the full standard library;
'' otherwise we can use a simple serial based library
pri {++complexio} file "libc/unix/vfs.c" _getrootvfs()
pri {++complexio} file "libc/unix/vfs.c" _setrootvfs(root)
pri {++complexio} file "libc/unix/_mount.c" _mount(name=string, volume)
pri {++complexio} file "libc/unix/_mount.c" _umount(name=string)
pri {++complexio} file "libc/unix/_mount.c" __getfilebuffer() : r=@byte
pri {++complexio} file "libc/unix/_mount.c" __getvfsforfile(name, orig_name, full_path)
pri {++complexio} file "libc/unix/exec.c" _execve(name=string, argv=0, envp=0)
pri {++complexio} file "libc/unix/exec.c" _fexecve(h, argv=0, envp=0)

'' I/O functions
pri {++complexio} file "libsys/fmt.c" _basic_open(h, sendf, recf, closef) : r=long
pri {++complexio} file "libsys/fmt.c" _basic_open_string(h, str, iomode) : r=long
pri {++complexio} file "libsys/fmt.c" _basic_close(h)
pri {++complexio} file "libc/unix/posixio.c" _freefile() : r=long
pri file "libsys/fmt.c" _basic_print_nl(h)
pri file "libsys/fmt.c" _basic_print_char(h, c, fmt = 0)
pri file "libsys/fmt.c" _basic_print_string(h, ptr, fmt = 0)
pri file "libsys/fmt.c" _basic_print_integer(h, x, fmt = 0, base=10)
pri file "libsys/fmt.c" _basic_print_integer_2(h, x1, x2, fmt = 0, base=10)
pri file "libsys/fmt.c" _basic_print_integer_3(h, x1, x2, x3, fmt = 0, base=10)
pri file "libsys/fmt.c" _basic_print_integer_4(h, x1, x2, x3, x4, fmt = 0, base=10)
pri file "libsys/fmt.c" _basic_print_unsigned(h, x, fmt = 0, base=10)
pri file "libsys/fmt.c" _basic_print_unsigned_2(h, x1, x2, fmt = 0, base=10)
pri file "libsys/fmt.c" _basic_print_unsigned_3(h, x1, x2, x3, fmt = 0, base=10)
pri file "libsys/fmt.c" _basic_print_unsigned_4(h, x1, x2, x3, x4, fmt = 0, base=10)
pri file "libsys/fmt.c" _basic_print_longinteger(h, x= >long, fmt = 0, base=10)
pri file "libsys/fmt.c" _basic_print_longunsigned(h, x= >+long, fmt = 0, base=10)
pri file "libsys/fmt.c" _basic_get_char(h)
pri file "libsys/fmt.c" _basic_put(h=long, pos=+long, ptr, elemnts=+long, size=+long)
pri file "libsys/fmt.c" _basic_get(h=long, pos=+long, ptr, elements=+long, size=+long)
pri file "libsys/fmt.c" __lockio(h=long) : r
pri file "libsys/fmt.c" __unlockio(h=long) : r

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
pri {++complexio} file "libsys/strings.bas" str`$(x=float): r=string
pri file "libsys/strings.bas" strint`$(x=long): r=string
pri file "libsys/strings.bas" bin`$(x, n=0): r=string
pri file "libsys/strings.bas" decuns`$(x, n=0): r=string
pri file "libsys/strings.bas" hex`$(x, n=0): r=string
pri file "libsys/strings.bas" oct`$(x, n=0): r=string
pri file "libsys/strings.bas" number`$(x, n=0, base=10): r=string

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

pri file "libsys/dir.bas" curdir`$(): r = string
pri file "libsys/dir.bas" dir`$(pat = "", attrib = 0): r=string
pri file "libsys/remove.c" _remove(f=string): r=long

pri file "libc/stdlib/errno.c" _seterror(r)
pri file "libc/stdlib/errno.c" _geterror(): r=long
pri file "libc/stdlib/errno.c" _geterrnoptr(): r=@long

pri file "libc/string/strerror.c" _strerror(e=long): r=string

pri file "libsys/c_startup.c" _c_startup()

pri _int64_signx(x = long) : rlo, rhi
  rlo := x
  rhi := x ~> 31  ' sar 31

pri _int64_zerox(x = long) : rlo, rhi
  rlo := x
  rhi := 0

pri _int64_and(alo, ahi, blo, bhi) : rlo, rhi
  rlo := alo & blo
  rhi := ahi & bhi

pri _int64_or(alo, ahi, blo, bhi) : rlo, rhi
  rlo := alo | blo
  rhi := ahi | bhi

pri _int64_xor(alo, ahi, blo, bhi) : rlo, rhi
  rlo := alo ^ blo
  rhi := ahi ^ bhi

pri _int64_neg(alo, ahi) : rlo, rhi
  rlo := !alo + 1
  rhi := !ahi - (rlo == 0)

  
pri _int64_abs(alo, ahi) : rlo, rhi
  if (ahi < 0)
    return _int64_sub(0, 0, alo, ahi)
  return alo, ahi

pri _int64_shl(alo, ahi, count, counthi) : rlo, rhi | tmp
  rlo := alo
  rhi := ahi
  if count & 32
    rhi := rlo
    rlo := 0
  if count & 31
    rhi := rhi << count
    tmp := rlo >> (-count)
    rhi |= tmp
    rlo := rlo << count

pri _int64_shr(alo, ahi, count, counthi) : rlo, rhi | tmp
  rlo := alo
  rhi := ahi
  if count & 32
    rlo := rhi
    rhi := 0
  if count & 31
    rlo := rlo >> count
    tmp := rhi << (-count)
    rlo |= tmp
    rhi := rhi >> count

pri _int64_sar(alo, ahi, count, counthi) : rlo, rhi | tmp
  rlo := alo
  rhi := ahi
  if count & 32
    rlo := rhi
    rhi := rlo~>31
  if count & 31
    rlo := rlo >> count
    tmp := rhi << (-count)
    rlo |= tmp
    rhi := rhi ~> count

pri _int64_muls(alo, ahi, blo, bhi) : rlo, rhi
  rlo := alo * blo
  rhi := alo +** blo
  rhi += ahi * blo
  rhi += bhi * alo

pri _int64_divmodu(nlo, nhi, dlo, dhi) : qlo, qhi, rlo, rhi | i, mask
  rlo := 0
  rhi := 0
  qlo := 0
  qhi := 0
  if (dlo == 0 and dhi == 0)
    qlo := qhi := $ffff_ffff
    rlo := nlo
    rhi := nhi    
    return qlo, qhi, rlo, rhi

  mask := $8000_0000
  'repeat i from 63 to 32
  repeat i from 31 to 0
    rlo,rhi := _int64_add(rlo, rhi, rlo, rhi)
    if nhi & mask
      rlo |= 1
    if _int64_cmpu(rlo, rhi, dlo, dhi) => 0
      rlo, rhi := _int64_sub(rlo, rhi, dlo, dhi)
      qhi |= mask
    mask := mask>>1

  mask := $8000_0000
  repeat i from 31 to 0
    rlo,rhi := _int64_add(rlo, rhi, rlo, rhi)
    if nlo & mask
      rlo |= 1
    if _int64_cmpu(rlo, rhi, dlo, dhi) => 0
      rlo, rhi := _int64_sub(rlo, rhi, dlo, dhi)
      qlo |= mask
    mask := mask>>1
  return qlo, qhi, rlo, rhi
  
pri _int64_divu(nlo, nhi, dlo, dhi) : qlo, qhi | x0, x1
  qlo, qhi, x0, x1 := _int64_divmodu(nlo, nhi, dlo, dhi)
  return qlo, qhi
  
pri _int64_modu(nlo, nhi, dlo, dhi) : rlo, rhi | x0, x1
  x0, x1, rlo, rhi := _int64_divmodu(nlo, nhi, dlo, dhi)
  return rlo, rhi

pri _int64_divmods(nlo, nhi, dlo, dhi) : qlo, qhi, rlo, rhi | sign
  if (nhi < 0)
    sign := 3
    nlo,nhi := _int64_neg(nlo, nhi)
  else
    sign := 0
  if (dhi < 0)
    sign ^= 2
    dlo,dhi := _int64_neg(dlo, dhi)
  qlo, qhi, rlo, rhi := _int64_divmodu(nlo,nhi,dlo,dhi)
  if (sign & 2)
    qlo,qhi := _int64_neg(qlo, qhi)
  if (sign & 1)
    rlo,rhi := _int64_neg(rlo, rhi)

pri _int64_divs(nlo, nhi, dlo, dhi) : qlo, qhi | x0, x1
  qlo, qhi, x0, x1 := _int64_divmods(nlo, nhi, dlo, dhi)
  return qlo, qhi

pri _int64_mods(nlo, nhi, dlo, dhi) : rlo, rhi | x0, x1
  x0, x1, rlo, rhi := _int64_divmods(nlo, nhi, dlo, dhi)
  return rlo, rhi

' Used to implement bit permutations on non-P2 targets
pri __builtin_bit_permute_step(x = +long,m = +long,shift = +long) : r = +long
  m &= (x >> shift) ^ x
  return m ^ (m << shift) ^ x

