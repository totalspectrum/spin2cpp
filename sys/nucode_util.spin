''
'' Nucode specific functions
''

pri _waitx(tim = long)
  __bytecode__("WAITX")

pri _pinr(pin = long) : val
  __bytecode__("PINR")
  
pri _getcnthl : rl = +long, rh = +long
  __bytecode__("GETCTHL")

pri _getcnt : r = +long | rh
  r, rh := _getcnthl()
  return r

pri _cogid : r = long
  __bytecode__("COGID")

pri _cogatn(mask = +long)
  __bytecode__("COGATN")

pri _pollatn : r = long
  __bytecode__("POLLATN")

pri _cogstop(x = long)
  __bytecode__("COGSTOP")

pri _coginit(id = long, code, param): r
  __bytecode__("COGINIT")
  
pri _cogchk(id = long) : r
  __bytecode__("COGCHK")

pri _drvl(pin = long)
  __bytecode__("DRVL")
pri _drvh(pin = long)
  __bytecode__("DRVH")
pri _drvnot(pin = long)
  __bytecode__("DRVNOT")
pri _drvrnd(pin = long)
  __bytecode__("DRVRND")
pri _drvw(pin = long, bit = long)
  __bytecode__("DRVWR")

pri _dirl(pin = long)
  __bytecode__("DIRL")
pri _dirh(pin = long)
  __bytecode__("DIRH")

pri _fltl(pin = long)
  __bytecode__("FLTL")

pri _wrpin(pin = long, val = long)
  __bytecode__("WRPIN")
pri _wxpin(pin = long, val = long)
  __bytecode__("WXPIN")
pri _wypin(pin = long, val = long)
  __bytecode__("WYPIN")
pri _rdpinx(pin = long) : r, c
  __bytecode__("RDPINX")
pri _rqpin(pin = long) : r
  __bytecode__("RQPIN")
pri _akpin(pin = long)
  __bytecode__("AKPIN")

pri _rdpin(pin = long) : r | c
  r,c := _rdpinx(pin)

' clear a smart pin
pri _pinclear(pin)
  _fltl(pin)
  _wrpin(pin, 0)

' synthetic smartpin instruction for setting up smartpin parameters
pri _pinsetup(pin = long, mode = long, xval, yval)
  _dirl(pin)
  _wrpin(pin, mode)
  _wxpin(pin, xval)
  _wypin(pin, yval)

pri _pinstart(pin = long, mode, xval, yval)
  _pinsetup(pin, mode, xval, yval)
  _dirh(pin)

pri _waitcnt(x = long)
  __bytecode__("WAITCNT")

pri waitcnt(x = long)
  __bytecode__("WAITCNT")
  
dat
    orgh
_rx_temp   long 0

con
 _rxpin = 63
 _txpin = 62

  _txmode       = %0000_0000_000_0000000000000_01_11110_0 'async tx mode, output enabled for smart output
  _rxmode       = %0000_0000_000_0000000000000_00_11111_0 'async rx mode, input  enabled for smart input

' utility function: check for tx finished
pri _txwait | w, c
  repeat
    w,c := _rdpinx(_txpin)
  while c <> 0

pri _setbaud(baudrate) | bitperiod, bit_mode
  _txwait()
  bitperiod := (__clkfreq_var / baudrate)
  _fltl(_txpin)
  _fltl(_rxpin)
  long[$1c] := baudrate
  bit_mode := 7 + (bitperiod << 16)
  _wrpin(_txpin, _txmode)
  _wxpin(_txpin, bit_mode)
  _wrpin(_rxpin, _rxmode)
  _wxpin(_rxpin, bit_mode)
  _drvl(_txpin)
  _drvl(_rxpin)
  
pri _txraw(c) | z
  _txwait()
  if long[$1c] == 0
    _setbaud(__default_baud__)  ' set up in common.c
  _wypin(_txpin, c)
  return 1

' timeout is approximately in milliseconds (actually in 1024ths of a second)
pri _rxraw(timeout = 0) : rxbyte = long | z, endtime, temp2, rxpin
  if long[$1c] == 0
    _setbaud(__default_baud__)
  if timeout
    endtime := _getcnt() + timeout * (__clkfreq_var >> 10)
  rxbyte := -1
  rxpin := _rxpin
  z := 0
  repeat
    z := _pinr(rxpin)
    if z
      rxbyte := _rdpin(rxpin)>>24
      quit
    if timeout
      if _getcnt() - endtime < 0
        quit

''
'' memset/memmove are here (in processor specific code)
'' because on P2 we can optimize them (long operations do
'' not have to be aligned)
''
pri __builtin_memset(ptr, val, count) : r | lval
  r := ptr
  lval := (val << 8) | val
  lval := (lval << 16) | lval
  repeat while (count > 3)
    long[ptr] := lval
    ptr += 4
    count -= 4
  repeat count
    byte[ptr] := val
    ptr += 1
    
pri __builtin_memmove(dst, src, count) : origdst
  origdst := dst
  if (dst < src)
    repeat while (count > 3)
      long[dst] := long[src]
      dst += 4
      src += 4
      count -= 4
    repeat count
      byte[dst] := byte[src]
      dst += 1
      src += 1
  else
    dst += count
    src += count
    repeat count
      dst -= 1
      src -= 1
      byte[dst] := byte[src]

'
' should these be builtins for NuCode as well?
'
pri longfill(ptr, val, count)
  repeat count
    long[ptr] := val
    ptr += 4
pri wordfill(ptr, val, count)
  repeat count
    word[ptr] := val
    ptr += 2
pri bytefill(ptr, val, count)
  __builtin_memset(ptr, val, count)

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

pri bytemove(dst, src, count)
  return __builtin_memmove(dst, src, count)

pri __builtin_memcpy(dst, src, count)
  return __builtin_memmove(dst, src, count)

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

pri _sqrt64(lo, hi) : r
  __bytecode__("SQRT64")

pri _sqrt(a) : r
  if (a =< 0)
    return 0
  return _sqrt64(a, 0)

pri _lockmem(addr)
  __bytecode__("LOCKMEM")

pri _locknew : rval
  __bytecode__("LOCKNEW")

pri _lockret(id)
  __bytecode__("LOCKRET")

pri _lockclr(id) : rval
  __bytecode__("LOCKCLR")

pri _lockset(id) : rval
  __bytecode__("LOCKSET")

pri _locktry(id) : rval
  __bytecode__("LOCKTRY")

pri _unlockmem(addr) | oldlock
  long[addr] := 0

pri __topofstack(ptr) : r
  return @ptr

pri __get_heap_base() : r
  __bytecode__("GETHEAP")

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

'
' random number generators
'
pri _lfsr_forward(x) : r
  __bytecode__("XORO")

pri _lfsr_backward(x) : r
  __bytecode__("XORO")

'
' time stuff
'
pri _getus() : freq = +long | lo, hi
  lo,hi := _getcnthl()
  freq := __clkfreq_us
  if freq == 0
    __clkfreq_us := freq := __clkfreq_var +/ 1000000
  hi := hi +// freq
  lo, hi := _div64(lo, hi, freq)
  return lo

pri _getms() : freq = +long | lo, hi
  lo,hi := _getcnthl()
  freq := __clkfreq_ms
  if freq == 0
    __clkfreq_ms := freq := __clkfreq_var +/ 1000
  hi := hi +// freq
  lo, hi := _div64(lo, hi, freq)
  return lo

pri _getsec() : freq = +long | lo, hi
  lo,hi := _getcnthl()
  freq := __clkfreq_var
  if freq == 0
    freq := 22_000_000
  hi := hi +// freq
  lo, hi := _div64(lo, hi, freq)
  return lo

pri _hubset(x)
  __bytecode__("HUBSET")
  
pri _clkset(mode, freq) | oldmode, xsel
  xsel := mode & 3
  if xsel == 0 and mode > 1
    xsel := 3
  oldmode := __clkmode_var & !3  ' remove low bits, if any
  __clkfreq_var := freq
  __clkmode_var := mode
  mode := mode & !3
  _hubset(oldmode)  ' go to RCFAST using known prior mode
  _hubset(mode)     ' setup for new mode, still RCFAST
  _waitx(20_000_000/100)
  mode |= xsel
  _hubset(mode)     ' activate new mode
  __clkfreq_ms := freq / 1000
  __clkfreq_us := freq / 1000000

pri _reboot
  _clkset(0, 0)
  _hubset(%0001 << 28)

pri _make_methodptr(o, func) | ptr
  ptr := _gc_alloc_managed(8)
  if (ptr)
    long[ptr] := o
    long[ptr+4] := func
  return ptr

''
'' divide (n, nlo) by d, producing qlo and rlo (used in FRAC operation)
''
pri _div64(n, nlo, dlo) : qlo, rlo
  __bytecode__("DIV64")

' signed version
pri _sdiv64(ahi, alo, b) : q,r | shi, slo, sign
  sign := 0
  if ahi < 0
    shi := 0
    slo := 0
    asm
      sub slo, alo wc
      subx shi, ahi
    endasm
    sign := 1
 else
   shi, slo := ahi, alo
 if b < 0
   sign ^= 1
   b := -b
   
 q,r := _div64(shi, slo, b)
 if sign
   q := -q

pri _muldiv64(mult1, mult2, divisor) : r
  __bytecode__("MULDIV64")
  
pri _qexp(v) : r
  __bytecode__("QEXP")

pri _qlog(v) : r
  __bytecode__("QLOG")

pri _ones(v = +long) : r = +long
  __bytecode__("ONES")

pri __builtin_parity(v = +long) : r = +long
  return _ones(v) & 1

pri _rotxy(x, y, angle) : nx, ny
  __bytecode__("ROTXY")

pri _xypol(x, y) : d, angle
  __bytecode__("XYPOL")

pri _qsin(len, angle, twopi) : y | x
  if twopi
    angle, x := _sdiv64(angle, 0, twopi)
  x, y := _rotxy(len, 0, angle)

pri _qcos(len, angle, twopi) : x | y
  if twopi
    angle, x := _sdiv64(angle, 0, twopi)
  x, y := _rotxy(len, 0, angle)

pri __builtin_movbyts(v = +long, m = +long) : r = +long
  __bytecode__("MOVBYTS")

pri __builtin_bswap16(x = +long) : y = +long
  y := __builtin_movbyts(x, %%3201)
  y &= $ffff

pri __builtin_bswap32(x = +long) : y = +long
  y := __builtin_movbyts(x, %%0123)

pri __builtin_mergew(x = +long) : y = +long
  __bytecode__("MERGEW")

pri __builtin_splitw(x = +long) : y = +long
  __bytecode__("SPLITW")

pri __builtin_mergeb(x = +long) : y = +long
  __bytecode__("MERGEB")

pri __builtin_splitb(x = +long) : y = +long
  __bytecode__("SPLITB")

pri __builtin_seussf(x = +long) : y = +long
  __bytecode__("SEUSSF")

pri __builtin_seussr(x = +long) : y = +long
  __bytecode__("SEUSSR")

pri _int64_add(alo, ahi, blo, bhi) : rlo, rhi
  __bytecode__("ADD64")

pri _int64_sub(alo, ahi, blo, bhi) : rlo, rhi
  __bytecode__("SUB64")

pri _int64_cmpu(alo, ahi, blo, bhi) : r
  __bytecode__("CMP64U")

pri _int64_cmps(alo, ahi, blo, bhi) : r
  __bytecode__("CMP64S")

