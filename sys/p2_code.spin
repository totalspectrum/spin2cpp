''
'' P2 specific system calls
''
pri _getcnt | r
  asm
    getct r
  endasm
  return r
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
pri cogid | rval
  asm
    cogid rval
  endasm
  return rval
pri cogstop(id)
  asm
    cogstop id
  endasm
  return 0
pri lockclr(id) | mask, rval
  mask := -1
  asm
    lockrel id wc
    muxc   rval,mask
  endasm
  return rval
pri lockset(id) | mask, rval
  mask := -1
  asm
    locktry id wc
    muxnc   rval,mask  ' NOTE: C bit is opposite in P2
  endasm
  return rval
pri locknew | rval
  asm
    locknew rval
  endasm
  return rval
pri lockret(id)
  asm
    lockret id
  endasm
  return 0

pri _reboot
  _clkset(0, 0)
  asm
    hubset ##%0001 << 28
  endasm
  
pri _clkset(mode, freq) | oldmode, xsel
  xsel := mode & 3
  if xsel == 0 and mode > 1
    xsel := 3
  oldmode := CLKMODE & !3  ' remove low bits, if any
  _clkfreq_var := freq
  _clkmode_var := mode
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

dat
    orgh
_bitcycles long 0

con
 _rxpin = 63
 _txpin = 62

  _txmode       = %0000_0000_000_0000000000000_01_11110_0 'async tx mode, output enabled for smart output
  _rxmode       = %0000_0000_000_0000000000000_00_11111_0 'async rx mode, input  enabled for smart input

pri _setbaud(baudrate) | bitperiod, bit_mode
  bitperiod := (CLKFREQ / baudrate)
  _dirl(_txpin)
  _dirl(_rxpin)
  _bitcycles := bitperiod
  bit_mode := 7 + (bitperiod << 16)
  wrpin(_txpin, _txmode)
  wxpin(_txpin, bit_mode)
  wrpin(_rxpin, _rxmode)
  wxpin(_rxpin, bit_mode)
  _dirh(_txpin)
  _dirh(_rxpin)
  
pri _txraw(c) | z
  if _bitcycles == 0
    _setbaud(230_400)
  wypin(_txpin, c)
  _waitx(1)
  repeat
    z := _pinr(_txpin)
  while z == 0
  return 1

pri _rxraw : rxbyte = long | z
  if _bitcycles == 0
    _setbaud(230_400)
  rxbyte := -1
  z := pinr(_rxpin)
  if z
    rxbyte := rdpin(_rxpin)>>24

pri _call_method(o, f, x=0) | r
  asm
    wrlong objptr, ptra
    add	   ptra, #4
    mov    objptr, o
    mov    arg01, x
    calla  f
    sub	   ptra, #4
    rdlong objptr, ptra
    mov    r, result1
  endasm
  return r
  
pri __builtin_propeller_dirl(pin)
  asm
    dirl pin
  endasm
pri __builtin_propeller_dirh(pin)
  asm
    dirh pin
  endasm
pri __builtin_propeller_dirnot(pin)
  asm
    dirnot pin
  endasm
pri __builtin_propeller_dirrnd(pin)
  asm
    dirrnd pin
  endasm
pri __builtin_propeller_dir(pin, c)
  asm
    test   c,#1 wc
    dirc pin
  endasm

pri __builtin_propeller_drvl(pin)
  asm
    drvl pin
  endasm
pri __builtin_propeller_drvh(pin)
  asm
    drvh pin
  endasm
pri __builtin_propeller_drvnot(pin)
  asm
    drvnot pin
  endasm
pri __builtin_propeller_drvrnd(pin)
  asm
    drvnot pin
  endasm
pri __builtin_propeller_drv(pin, c)
  asm
    test   c,#1 wc
    drvc pin
  endasm

pri __builtin_propeller_outl(pin)
  asm
    outl pin
  endasm
pri __builtin_propeller_outh(pin)
  asm
    outh pin
  endasm
pri __builtin_propeller_outnot(pin)
  asm
    outnot pin
  endasm
pri __builtin_propeller_outrnd(pin)
  asm
    outnot pin
  endasm
pri __builtin_propeller_out(pin, c)
  asm
    test   c,#1 wc
    outc pin
  endasm

pri __builtin_propeller_fltl(pin)
  asm
    fltl pin
  endasm
pri __builtin_propeller_flth(pin)
  asm
    flth pin
  endasm
pri __builtin_propeller_fltnot(pin)
  asm
    fltnot pin
  endasm
pri __builtin_propeller_fltrnd(pin)
  asm
    fltnot pin
  endasm
pri __builtin_propeller_flt(pin, val)
  asm
    test   val,#1 wc
    fltc pin
  endasm

pri __builtin_propeller_pinr(pin) : val
  asm
    testp   pin wc
    wrc val
  endasm
  
pri __builtin_propeller_wrpin(val, pin)
  asm
    wrpin val, pin
  endasm
pri __builtin_propeller_wxpin(val, pin)
  asm
    wxpin val, pin
  endasm
pri __builtin_propeller_wypin(val, pin)
  asm
    wypin val, pin
  endasm
pri __builtin_propeller_waitx(tim)
  asm
    waitx tim
  endasm

pri __builtin_propeller_cogatn(mask)
  asm
    cogatn mask
  endasm
pri __builtin_propeller_rdpin(pin) : r
  asm
    rdpin r, pin
  endasm
pri __builtin_propeller_rqpin(pin) : r
  asm
    rqpin q, pin
  endasm
pri __builtin_propeller_akpin(pin)
  asm
    akpin pin
  endasm

pri _rnd : r
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
pri _pinmode(pin, mode, xval, yval)
  _dirl(pin)
  _wrpin(pin, mode)
  _wxpin(pin, xval)
  _wypin(pin, yval)
  _dirh(pin)
  
' versions of smartpin instructions with the Spin order of parameters
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
