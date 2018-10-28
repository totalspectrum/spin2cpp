''
'' P2 specific system calls
''

pri cnt | r
  asm
    getct r
  endasm
  return r
pri getcnt | r
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
pri clkset(mode, freq)
  CLKFREQ := freq
  CLKMODE := mode
  asm
    hubset mode
  endasm
pri reboot
  clkset($80, 0)
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
  endasm
  return id
con
 _rxpin = 31
 _txpin = 30
 _bitcycles = 80_000_000 / 115_200
pri _tx(c) | val, nextcnt
  DIRB[_txpin] := 1
  OUTB[_txpin] := 1
  val := (c | 256) << 1
  nextcnt := cnt
  repeat 10
    waitcnt(nextcnt += _bitcycles)
    OUTB[_txpin] := val
    val >>= 1
pri _rx | val, waitcycles, i
  DIRB[_rxpin] := 0
  repeat
  while INB[_rxpin] <> 0
  waitcycles := cnt + (_bitcycles>>1)
  val := 0
  repeat 8
    waitcnt(waitcycles += _bitcycles)
    val := (INB[_rxpin] << 7) | (val>>1)
  waitcnt(waitcycles + _bitcycles)
  _tx(val)
  return val
