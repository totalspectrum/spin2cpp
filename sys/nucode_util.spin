''
'' Nucode specific functions
''

'' FIXME: these are dummys
pri _rxraw(timeout = 0)
  if timeout
    timeout *= __clkfreq_var >> 10
  return -1

''
'' divide (n, nlo) by d, producing qlo and rlo (used in FRAC operation)
''
pri _div64(n, nlo, dlo) : qlo, rlo | q, r, d
  return 0,0

pri _waitx(tim)
  __bytecode__("WAITX")

pri _pinr(pin) : val
  __bytecode__("PINR")
  
pri _getcnthl : rl = +long, rh = +long
  __bytecode__("GETCTHL")

pri _getcnt : r = +long | rh
  r, rh := _getcnthl()
  return r

pri _cogid : r = long
  __bytecode__("COGID")

pri _cogstop(x)
  __bytecode__("COGSTOP")

pri _cogchk(id) : r
  __bytecode__("COGCHK")

pri _muldiv64(mult1, mult2, divisor) : r
  __bytecode__("MULDIV64")
  
pri _drvl(pin)
  __bytecode__("DRVL")
pri _drvh(pin)
  __bytecode__("DRVH")
pri _drvnot(pin)
  __bytecode__("DRVNOT")
pri _drvrnd(pin)
  __bytecode__("DRVRND")

pri _dirl(pin)
  __bytecode__("DIRL")
pri _dirh(pin)
  __bytecode__("DIRH")

pri _fltl(pin)
  __bytecode__("FLTL")

pri _wrpin(pin, val)
  __bytecode__("WRPIN")
pri _wxpin(pin, val)
  __bytecode__("WXPIN")
pri _wypin(pin, val)
  __bytecode__("WYPIN")
  
pri _waitcnt(x)
  __bytecode__("WAITCNT")

pri waitcnt(x)
  __bytecode__("WAITCNT")
  
dat
    orgh
_rx_temp   long 0

con
 _rxpin = 63
 _txpin = 62

  _txmode       = %0000_0000_000_0000000000000_01_11110_0 'async tx mode, output enabled for smart output
  _rxmode       = %0000_0000_000_0000000000000_00_11111_0 'async rx mode, input  enabled for smart input

pri _setbaud(baudrate) | bitperiod, bit_mode
  bitperiod := (__clkfreq_var / baudrate)
  _dirl(_txpin)
  _dirl(_rxpin)
  long[$1c] := baudrate
  bit_mode := 7 + (bitperiod << 16)
  _wrpin(_txpin, _txmode)
  _wxpin(_txpin, bit_mode)
  _wrpin(_rxpin, _rxmode)
  _wxpin(_rxpin, bit_mode + 20)  ' async using 28 bits instead of 8
  _dirh(_txpin)
  _dirh(_rxpin)
  
pri _txraw(c) | z
  if long[$1c] == 0
    _setbaud(__default_baud__)  ' set up in common.c
  _wypin(_txpin, c)
  repeat
    z := _pinr(_txpin)
  while z == 0
  return 1

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

pri _lockmem(addr)
  __bytecode__("LOCKMEM")

pri _unlockmem(addr) | oldlock
  long[addr] := 0

pri __topofstack(ptr)
  return @ptr

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
  