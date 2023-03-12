''
'' P2 specific system calls
''
pri _getcnt : r = +long
  asm
    getct r
  endasm
  return r
pri _getcnth : r = +long
  asm
    getct r wc
  endasm
  return r
pri _getcnthl : rl = +long, rh = +long
  asm
    getct rh wc
    getct rl
  endasm
  return rl, rh
  
pri waitcnt(x = long)
  asm
    addct1  x, #0
    waitct1
  endasm
pri _waitcnt(x = long)
  asm
    addct1  x, #0
    waitct1
  endasm

pri _cogid : rval = long
  asm
    cogid rval
  endasm
  return rval
pri _cogstop(id = long)
  asm
    cogstop id
  endasm
  return 0
pri _lockclr(id = long) : rval | mask
  mask := -1
  asm
    lockrel id wc
    muxc   rval,mask
  endasm
  return rval
pri _lockset(id = long) : rval | mask
  mask := -1
  asm
    locktry id wc
    muxnc   rval,mask  ' NOTE: C bit is opposite in P2
  endasm
  return rval

' locktry is like lockset but with opposite return value
pri _locktry(id = long) : rval |  mask
  mask := -1
  asm
    locktry id wc
    muxc   rval,mask
  endasm
  return rval
pri _locknew : rval = long
  asm
    locknew rval
  endasm
  return rval
pri _lockret(id = long)
  asm
    lockret id
  endasm
  return 0

pri _reboot
  _clkset(0, 0)
  asm
    hubset ##%0001 << 28
  endasm

pri _hubset(x)
  asm
    hubset x
  endasm
  
pri _clkset(mode, freq) | oldmode, xsel
  xsel := mode & 3
  if xsel == 0 and mode > 1
    xsel := 3
  oldmode := __clkmode_var & !3  ' remove low bits, if any
  __clkfreq_var := freq
  __clkmode_var := mode
  mode := mode & !3
  asm
    hubset oldmode      ' go to RCFAST using known prior mode
    hubset mode         ' setup for new mode, still RCFAST
    waitx ##20_000_000/100 ' wait ~10ms
    or  mode, xsel
    hubset mode         ' activate new mode
  endasm
  __clkfreq_ms := freq / 1000
  __clkfreq_us := freq / 1000000
  _bitcycles := 0

pri _sqrt(a) | r
  if (a =< 0)
    return 0
  asm
    qsqrt a, #0
    getqx r
  endasm
  return r
pri _sqrt64(lo, hi) | r
  asm
    qsqrt lo, hi
    getqx r
  endasm
  return r
pri _coginit(id, code, param)
  asm
    setq param
    coginit id, code wc
 if_c neg id, #1
  endasm
  return id

pri _cogchk(id = long) : r
  r := -1
  asm
    cogid id wc
 if_nc mov r, #0 
  endasm
  return r

''
'' serial send/receive code
'' this code uses smarpins, so doesn't require a COG. The down side is that
'' the smartpin has only a single character buffer.
'' Following a suggestion from forum user evanh, we improve this for receive
'' by putting the smartpin into 28 bit mode rather than 8 bit, and then manually
'' checking for start/stop bits in the bottom 20 bits. This effectively
'' creates a 6 character buffer: 3 characters in the Z register, and 3 characters
'' internally to the smartpin
''
dat
    orgh
_bitcycles long 0
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
  bitperiod := (__clkfreq_var / baudrate)
  _txwait()
  _dirl(_txpin)
  _dirl(_rxpin)
  _bitcycles := bitperiod
  bit_mode := 7 + (bitperiod << 16)
  _wrpin(_txpin, _txmode)
  _wxpin(_txpin, bit_mode)
  _wrpin(_rxpin, _rxmode)
  _wxpin(_rxpin, bit_mode + 20)  ' async using 28 bits instead of 8
  _dirh(_txpin)
  _dirh(_rxpin)


pri _txraw(c) | z
  _txwait()
  if _bitcycles == 0
    _setbaud(__default_baud__)  ' set up in common.c
  _wypin(_txpin, c)
  return 1

' timeout is approximately in milliseconds (actually in 1024ths of a second)
pri _rxraw(timeout = 0) : rxbyte = long | z, endtime, temp2, rxpin
  if _bitcycles == 0
    _setbaud(__default_baud__)
  if timeout
    endtime := _getcnt() + timeout * (__clkfreq_var >> 10)
  else
    endtime := 0 ' just gets rid of a compiler warning
  rxbyte := -1
  rxpin := _rxpin
  z := 0
  temp2 := _rx_temp
  '' slightly tricky code for pulling out the bytes from the 28 bits
  '' of data presented by the smartpin
  '' Courtesy of evanh
  repeat
    asm
          testb  temp2, #8 wc     ' check framing of prior character for valid character   
          testbn temp2, #9 andc   ' more framing check (1 then 0)
          shr    temp2, #10       ' shift down next character, if any
  if_c    mov    z, #1
  if_c    jmp    #.breakone
          testp  rxpin wz
  if_z    mov    z, #1
  if_z    rdpin  temp2, rxpin
  if_z    shr    temp2, #32 - 28
.breakone
    endasm
  until z or (timeout and (_getcnt() - endtime < 0))
  if z
    rxbyte := temp2 & $ff
  _rx_temp := temp2
  
pri _call_method(o, f, x=0) | r
  asm
    wrlong objptr, ptra
    add    ptra, #4
    mov    objptr, o
    mov    arg01, x
    call   f
    sub    ptra, #4
    rdlong objptr, ptra
    mov    r, result1
  endasm
  return r
  
pri _dirl(pin = long)
  asm
    dirl pin
  endasm
pri _dirh(pin = long)
  asm
    dirh pin
  endasm
pri _dirnot(pin = long)
  asm
    dirnot pin
  endasm
pri _dirrnd(pin = long)
  asm
    dirrnd pin
  endasm
pri _dirw(pin = long, c = long)
  asm
    test   c,#1 wc
    dirc pin
  endasm

pri _drvl(pin = long)
  asm
    drvl pin
  endasm
pri _drvh(pin = long)
  asm
    drvh pin
  endasm
pri _drvnot(pin = long)
  asm
    drvnot pin
  endasm
pri _drvrnd(pin = long)
  asm
    drvrnd pin
  endasm
pri _drvw(pin = long, c = long)
  asm
    test   c,#1 wc
    drvc pin
  endasm

pri _outl(pin = long)
  asm
    outl pin
  endasm
pri _outh(pin = long)
  asm
    outh pin
  endasm
pri _outnot(pin = long)
  asm
    outnot pin
  endasm
pri _outrnd(pin = long)
  asm
    outnot pin
  endasm
pri _out(pin = long, c = long)
  asm
    test   c,#1 wc
    outc pin
  endasm

pri _fltl(pin = long)
  asm
    fltl pin
  endasm
pri _flth(pin = long)
  asm
    flth pin
  endasm
pri _fltnot(pin = long)
  asm
    fltnot pin
  endasm
pri _fltrnd(pin = long)
  asm
    fltnot pin
  endasm
pri _fltw(pin = long, val = long)
  asm
    test   val,#1 wc
    fltc pin
  endasm

' special case of _pinread where "pin" is a single pin
pri _pinr(pin = long) : val
  asm
    testp   pin wc
    wrc val
  endasm
  
pri _waitx(tim = long)
  asm
    waitx tim
  endasm

pri _cogatn(mask = +long)
  asm
    cogatn mask
  endasm

pri _pollatn() : flag
  flag := 0
  asm
    pollatn wc
    subx flag, #0  ' turns c bit into -1
  endasm

pri _waitatn()
  asm
    waitatn
  endasm
  
pri _rdpin(pin = long) : r
  asm
    rdpin r, pin
  endasm
pri _rdpinx(pin = long) : r, c
  c := 0
  asm
    rdpin r, pin wc
if_c neg c, #1
  endasm
  
pri _rqpin(pin = long) : r
  asm
    rqpin r, pin
  endasm
pri _akpin(pin = long)
  asm
    akpin pin
  endasm

pri _rnd : r = +long
  asm
    getrnd r
  endasm

pri _getrnd : r = +long
  asm
    getrnd r
  endasm
  
pri _rotxy(x, y, angle)
  asm
    setq y
    qrotate x, angle
    getqx x
    getqy y
  endasm
  return x,y

pri _polxy(d, angle) : x, y
  asm
    qrotate d, angle
    getqx x
    getqy y
  endasm

' divide (ahi, alo) by b to produce 32 bit quotient q and remainder r
' unsigned version
pri _div64(ahi, alo, b) : q,r
  asm
    setq ahi
    qdiv alo, b
    getqx q
    getqy r
  endasm

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

' various
pri _xypol(x, y) : d, angle
  asm
    qvector x, y
    getqx d
    getqy angle
  endasm

pri _qsin(len, angle, twopi) : y | x
  if twopi
    if angle > twopi __orelse__ angle < -twopi
      angle := angle // twopi
    angle, x := _sdiv64(angle, 0, twopi)
  x, y := _rotxy(len, 0, angle)

pri _qcos(len, angle, twopi) : x | y
  if twopi
    if angle > twopi __orelse__ angle < -twopi
      angle := angle // twopi
    angle, x := _sdiv64(angle, 0, twopi)
  x, y := _rotxy(len, 0, angle)
  
' synthetic smartpin instruction for setting up smartpin parameters
pri _pinsetup(pin, mode, xval, yval)
  _dirl(pin)
  _wrpin(pin, mode)
  _wxpin(pin, xval)
  _wypin(pin, yval)

pri _pinstart(pin, mode, xval, yval)
  _pinsetup(pin, mode, xval, yval)
  _dirh(pin)

pri _pinclear(pin)
  asm
    fltl pin
    wrpin #0, pin
  endasm
  
' NOTE: the order of parameters is reversed from the instruction
pri _wrpin(pin, val)
  asm
    wrpin val, pin
  endasm
pri _wxpin(pin, val)
  asm
    wxpin val, pin
  endasm
pri _wypin(pin, val)
  asm
    wypin val, pin
  endasm

pri _muldiv64(mult1, mult2, divisor) : r
  asm
    qmul mult1, mult2
    getqy mult1
    getqx mult2
    setq mult1
    qdiv mult2, divisor
    getqx r
  endasm

pri _call(hubaddr)
  asm
    call hubaddr
  endasm

pri _ones(v = +long) : r=+long
  asm
    ones r, v
  endasm

pri __builtin_parity(v = +long) : r = +long
  asm
    or   v, #0 wc
    wrc  r
  endasm
  
pri __builtin_bswap16(v = +long) : r = +long
  r := v
  asm
    movbyts r, #%%3201
    zerox   r, #15
  endasm

pri __builtin_bswap32(v = +long) : r = +long
  r := v
  asm
    movbyts r, #%%0123
  endasm

pri __builtin_movbyts(v = +long, m = +long) : r = +long
  r := v
  asm
    movbyts r, m
  endasm

pri __builtin_mergew(v = +long) : r = +long
  r := v
  asm
    mergew r
  endasm

pri __builtin_splitw(v = +long) : r = +long
  r := v
  asm
    splitw r
  endasm

pri __builtin_mergeb(v = +long) : r = +long
  r := v
  asm
    mergeb r
  endasm

pri __builtin_splitb(v = +long) : r = +long
  r := v
  asm
    splitb r
  endasm

pri __builtin_seussf(v = +long) : r = +long
  r := v
  asm
    seussf r
  endasm

pri __builtin_seussr(v = +long) : r = +long
  r := v
  asm
    seussr r
  endasm
  
pri _qexp(v) : r
  asm
    qexp v
    getqx r
  endasm

pri _qlog(v) : r
  asm
    qlog v
    getqx r
  endasm

pri _getsec() : freq = +long | hi, lo
  freq := __clkfreq_var
  asm
    getct hi wc
    getct lo
    setq hi
    qdiv lo, freq
    getqx lo
  endasm
  return lo

pri _getms() : freq = +long | lo, hi
  lo,hi := _getcnthl()
  freq := __clkfreq_ms
  if freq == 0
    __clkfreq_ms := freq := __clkfreq_var +/ 1000
  asm
    setq hi
    qdiv lo, freq
    getqx freq
  endasm
  return freq

pri _getus() : freq = +long | lo, hi
  lo,hi := _getcnthl()
  freq := __clkfreq_us
  if freq == 0
    __clkfreq_us := freq := __clkfreq_var +/ 1000000
  asm
    qdiv hi, freq
    getqy hi
    setq hi
    qdiv lo, freq
    getqx freq
  endasm
  return freq

''
'' memset/memmove are here (in processor specific code)
'' because on P2 we can optimize them (long operations do
'' not have to be aligned)
''
pri {++specialfunc(memset)} __builtin_memset(ptr, val, count) : r | lval
  r := ptr
  asm
    call #\builtin_bytefill_
  endasm

pri __builtin_longset(ptr, val, count) : r | lval
  r := ptr
  asm
    call #\builtin_longfill_
  endasm

pri __builtin_memmove(dst, src, count) : origdst
  origdst := dst
  if dst < src __orelse__ dst => src+count
    repeat count >> 2
      long[dst] := long[src]
      dst += 4
      src += 4
    if count & 2
      word[dst] := word[src]
      dst += 2
      src += 2
    if count & 1
      byte[dst] := byte[src]
  else
    dst += count
    src += count
    repeat count
      dst -= 1
      src -= 1
      byte[dst] := byte[src]

''
'' bytefill etc.
''
pri bytefill(ptr, val, count)
  asm
    call #\builtin_bytefill_
  endasm
  
pri wordfill(ptr, val, count)
  asm
    call #\builtin_wordfill_
  endasm

pri longfill(ptr, val, count)
  asm
    call #\builtin_longfill_
  endasm

''
'' 64 bit operations
''
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

' compare unsigned alo, ahi, return -1, 0, or +1
pri _int64_cmpu(alo, ahi, blo, bhi) : r
  asm
      cmp  alo, blo wc,wz
      cmpx ahi, bhi wc,wz
if_z  mov  r, #0
if_nz negc r, #1
  endasm
  
' compare signed alo, ahi, return -1, 0, or +1
pri _int64_cmps(alo, ahi, blo, bhi) : r
  asm
      cmp  alo, blo wc,wz
      cmpsx ahi, bhi wc,wz
if_z  mov  r, #0
if_nz negc r, #1
  endasm

