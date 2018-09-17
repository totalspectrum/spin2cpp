pri waitcnt(x)
  asm
    waitcnt x,#0
  endasm

pri getcnt
  return cnt

pri waitpeq(pin, mask, c = 0)
  asm
    waitpeq pin,mask
  endasm

pri waitpne(pin, mask, c = 0)
  asm
    waitpne pin,mask
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

pri clkset(mode, freq)
  CLKFREQ := freq
  CLKMODE := mode
  asm
    clkset mode
  endasm

pri reboot
  clkset($80, 0)

pri lockclr(id) | mask, rval
  mask := -1
  asm
    lockclr id wc
    muxc   rval,mask
  endasm
  return rval

pri lockset(id) | mask, rval
  mask := -1
  asm
    lockset id wc
    muxc   rval,mask
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

pri _coginit(id, code, param) | parm
  parm := (param & $fffc) << 16
  parm |= (code & $fffc) << 2
  parm | = id & $f
  asm
    coginit parm wr
  endasm
  return parm

pri _sqrt(a) | r, bit, tmp
  if (a =< 0)
    return 0
  r := 0
  bit := (1<<30)
  repeat while (bit > a)
    bit := bit >> 2
  repeat while (bit <> 0)
    tmp := r+bit
    if (a => tmp)
      a := a - tmp
      r := (r >> 1) + bit
    else
      r := r >> 1
    bit := bit >> 2
  return r

con
 _txpin = 30
 _bitcycles = 80_000_000 / 115_200

pri _tx(c) | val, nextcnt
  OUTA[_txpin] := 1
  DIRA[_txpin] := 1
  val := (c | 256) << 1
  nextcnt := cnt
  repeat 10
    waitcnt(nextcnt += _bitcycles)
    OUTA[_txpin] := val
    val >>= 1
