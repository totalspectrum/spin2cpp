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
  
pri waitcnt(x)
  asm
    addct1  x, #0
    waitct1
  endasm
pri _waitcnt(x)
  asm
    addct1  x, #0
    waitct1
  endasm

pri _cogid | rval
  asm
    cogid rval
  endasm
  return rval
pri _cogstop(id)
  asm
    cogstop id
  endasm
  return 0
pri _lockclr(id) | mask, rval
  mask := -1
  asm
    lockrel id wc
    muxc   rval,mask
  endasm
  return rval
pri _lockset(id) | mask, rval
  mask := -1
  asm
    locktry id wc
    muxnc   rval,mask  ' NOTE: C bit is opposite in P2
  endasm
  return rval

' locktry is like lockset but with opposite return value
pri _locktry(id) | mask, rval
  mask := -1
  asm
    locktry id wc
    muxc   rval,mask
  endasm
  return rval
pri _locknew | rval
  asm
    locknew rval
  endasm
  return rval
pri _lockret(id)
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
    hubset oldmode	' go to RCFAST using known prior mode
    hubset mode		' setup for new mode, still RCFAST
    waitx ##20_000_000/100 ' wait ~10ms
    or  mode, xsel
    hubset mode         ' activate new mode
  endasm
pri _sqrt(a) | r
  if (a =< 0)
    return 0
  asm
    qsqrt a, #0
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

pri _cogchk(id) : r
  r := -1
  asm
    cogid id wc
 if_nc mov r, #0 
  endasm
  return r

dat
    orgh
_bitcycles long 0

con
 _rxpin = 63
 _txpin = 62

  _txmode       = %0000_0000_000_0000000000000_01_11110_0 'async tx mode, output enabled for smart output
  _rxmode       = %0000_0000_000_0000000000000_00_11111_0 'async rx mode, input  enabled for smart input

pri _setbaud(baudrate) | bitperiod, bit_mode
  bitperiod := (__clkfreq_var / baudrate)
  _dirl(_txpin)
  _dirl(_rxpin)
  _bitcycles := bitperiod
  bit_mode := 7 + (bitperiod << 16)
  _wrpin(_txpin, _txmode)
  _wxpin(_txpin, bit_mode)
  _wrpin(_rxpin, _rxmode)
  _wxpin(_rxpin, bit_mode)
  _dirh(_txpin)
  _dirh(_rxpin)
  
pri _txraw(c) | z
  if _bitcycles == 0
    _setbaud(230_400)
  _wypin(_txpin, c)
  _waitx(1)
  repeat
    z := _pinr(_txpin)
  while z == 0
  return 1

' timeout is approximately in milliseconds (actually in 1024ths of a second)
pri _rxraw(timeout = 0) : rxbyte = long | z, endtime
  if _bitcycles == 0
    _setbaud(230_400)
  if timeout
    endtime := _getcnt() + timeout * (__clkfreq_var >> 10)
  rxbyte := -1
  repeat
    z := _pinr(_rxpin)
  until z or (timeout and (_getcnt() - endtime < 0))
  if z
    rxbyte := _rdpin(_rxpin)>>24

pri _call_method(o, f, x=0) | r
  asm
    wrlong objptr, ptra
    add	   ptra, #4
    mov    objptr, o
    mov    arg01, x
    call   f
    sub	   ptra, #4
    rdlong objptr, ptra
    mov    r, result1
  endasm
  return r
  
pri _dirl(pin)
  asm
    dirl pin
  endasm
pri _dirh(pin)
  asm
    dirh pin
  endasm
pri _dirnot(pin)
  asm
    dirnot pin
  endasm
pri _dirrnd(pin)
  asm
    dirrnd pin
  endasm
pri _dirw(pin, c)
  asm
    test   c,#1 wc
    dirc pin
  endasm

pri _drvl(pin)
  asm
    drvl pin
  endasm
pri _drvh(pin)
  asm
    drvh pin
  endasm
pri _drvnot(pin)
  asm
    drvnot pin
  endasm
pri _drvrnd(pin)
  asm
    drvnot pin
  endasm
pri _drvw(pin, c)
  asm
    test   c,#1 wc
    drvc pin
  endasm

pri _outl(pin)
  asm
    outl pin
  endasm
pri _outh(pin)
  asm
    outh pin
  endasm
pri _outnot(pin)
  asm
    outnot pin
  endasm
pri _outrnd(pin)
  asm
    outnot pin
  endasm
pri _out(pin, c)
  asm
    test   c,#1 wc
    outc pin
  endasm

pri _fltl(pin)
  asm
    fltl pin
  endasm
pri _flth(pin)
  asm
    flth pin
  endasm
pri _fltnot(pin)
  asm
    fltnot pin
  endasm
pri _fltrnd(pin)
  asm
    fltnot pin
  endasm
pri _fltw(pin, val)
  asm
    test   val,#1 wc
    fltc pin
  endasm

pri _pinr(pin) : val
  asm
    testp   pin wc
    wrc val
  endasm
  
pri _waitx(tim)
  asm
    waitx tim
  endasm

pri _cogatn(mask)
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
  
pri _rdpin(pin) : r
  asm
    rdpin r, pin
  endasm
pri _rqpin(pin) : r
  asm
    rqpin r, pin
  endasm
pri _akpin(pin)
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

pri _xypol(x, y) : d, angle
  asm
    qvector x, y
    getqx d
    getqy angle
  endasm

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

pri _ones(v) : r
  asm
    ones r, v
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
  freq := __clkfreq_var
  asm
    qdiv freq, ##1000
    getqx freq
    setq hi
    qdiv lo, freq
    getqx freq
  endasm
  return freq

pri _getus() : freq = +long | lo, hi
  lo,hi := _getcnthl()
  freq := __clkfreq_var
  asm
    qdiv freq, ##1000000
    getqx freq
    setq hi
    qdiv lo, freq
    getqx freq
  endasm
  return freq

''
'' bytefill/bytemove are here (in processor specific code)
'' because on P2 we can optimize them (long operations do
'' not have to be aligned)
''
pri bytefill(ptr, val, count) | lval
  lval := (val << 8) | val
  lval := (lval << 16) | lval
  repeat while (count > 3)
    long[ptr] := lval
    ptr += 4
    count -= 4
  repeat count
    byte[ptr] := val
    ptr += 1
    
pri bytemove(dst, src, count) : origdst
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
      
