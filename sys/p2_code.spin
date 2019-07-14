''
'' P2 specific system calls
''

pri cnt | r
  asm
    getct r
  endasm
  return r
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
pri clkset(mode, freq, xsel = 3) | oldfreq
  oldfreq := CLKFREQ & !3  ' remove low bits, if any
  CLKFREQ := freq
  CLKMODE := mode
  asm
    hubset oldfreq	' go to RCFAST using known prior mode
    hubset mode		' setup for new mode, still RCFAST
    waitx ##20_000_000/100 ' wait ~10ms
    add  mode, xsel
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
_bitcycles long 0

pri _setbaud(rate)
  _bitcycles := clkfreq / rate
  _baudrate := rate '' update global config area
  
con
 _rxpin = 31
 _txpin = 30
pri _tx(c) | val, nextcnt, bitcycles
  bitcycles := _bitcycles
  if (bitcycles == 0)
    if clkfreq == 0
      clkset($010007f8, 160_000_000)
    _setbaud(230_800)
    bitcycles := _bitcycles
  DIRB[_txpin] := 1
  OUTB[_txpin] := 1
  val := (c | 256) << 1
  nextcnt := cnt
  repeat 10
    waitcnt(nextcnt += bitcycles)
    OUTB[_txpin] := val
    val >>= 1
pri _rx | val, waitcycles, i, bitcycles
  bitcycles := _bitcycles
  DIRB[_rxpin] := 0
  repeat
  while INB[_rxpin] <> 0
  waitcycles := cnt + (bitcycles>>1)
  val := 0
  repeat 8
    waitcnt(waitcycles += bitcycles)
    val := (INB[_rxpin] << 7) | (val>>1)
  waitcnt(waitcycles + bitcycles)
  _tx(val)
  return val
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
    test   pin,#1 wc
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
    test   pin,#1 wc
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
    test   pin,#1 wc
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
pri __builtin_propeller_flt(pin, c)
  asm
    test   pin,#1 wc
    fltc pin
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