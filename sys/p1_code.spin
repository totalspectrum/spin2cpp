pri waitcnt(x)
  asm
    waitcnt x,#0
  endasm

pri _getcnt : r = +long
  r := cnt

pri waitpeq(pin, mask, c = 0)
  asm
    waitpeq pin,mask
  endasm

pri waitpne(pin, mask, c = 0)
  asm
    waitpne pin,mask
  endasm

pri waitvid(colors, pixels)
  asm
    waitvid colors, pixels
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

pri _clkset(mode, freq)
  __clkfreq_var := freq
  __clkmode_var := mode
  asm
    clkset mode
  endasm

pri _reboot
  _clkset($80, 0)

pri _lockclr(id) | mask, rval
  mask := -1
  asm
    lockclr id wc
    muxc   rval,mask
  endasm
  return rval

pri _lockset(id) | mask, rval
  mask := -1
  asm
    lockset id wc
    muxc   rval,mask
  endasm
  return rval

' like lockset but has opposite return value
pri _locktry(id) | mask, rval
  mask := -1
  asm
    lockset id wc
    muxnc   rval,mask
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

pri _coginit(id, code, param) | parm
  parm := (param & $fffc) << 16
  parm |= (code & $fffc) << 2
  parm | = id & $f
  asm
      coginit parm wr, wc
 if_c neg     parm, #1
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
 _rxpin = 31
 _txpin = 30

dat
_bitcycles long 80_000_000 / 115_200

pri _txraw(c) | val, nextcnt, bitcycles
  bitcycles := _bitcycles
  outa[_txpin] := 1
  dira[_txpin] := 1
  val := (c | 256) << 1
  nextcnt := cnt
  repeat 10
    waitcnt(nextcnt += bitcycles)
    outa[_txpin] := val
    val >>= 1
  return 1

' timeout is in 1024ths of a second (roughly milliseconds)
pri _rxraw(timeout = 0) | val, waitcycles, i, bitcycles
  bitcycles := _bitcycles
  dira[_rxpin] := 0

  if timeout
    waitcycles := cnt + timeout * (__clkfreq_var >> 10)   
    repeat
      if ina[_rxpin] == 0
        quit
      if waitcycles - cnt < 0
        return -1
  else
    repeat until ina[_rxpin] == 0
    
  waitcycles := cnt + (bitcycles>>1)
  val := 0
  repeat 8
    waitcnt(waitcycles += bitcycles)
    val := (ina[_rxpin] << 7) | (val>>1)
  waitcnt(waitcycles + bitcycles)
  return val

pri _setbaud(rate)
  _bitcycles := __clkfreq_var / rate

pri _call_method(o, f, x=0) | r
  asm
    wrlong objptr, sp
    add    sp, #4
    mov    objptr, o
    mov    arg01, x
    call   f
    sub    sp, #4
    rdlong objptr, sp
    mov    r, result1
  endasm
  return r

pri _fltl(pin) | mask
  mask := 1<<pin
  dira &= !mask
  outa &= !mask
  
pri _flth(pin) | mask
  mask := 1<<pin
  dira &= !mask
  outa |= mask

pri _dirl(pin) | mask
  mask := 1<<pin
  dira &= !mask

pri _dirh(pin) | mask
  mask := 1<<pin
  dira |= mask

pri _dirnot(pin) | mask
  mask := 1<<pin
  dira ^= mask

pri _dirw(pin, c) | mask
  mask := 1<<pin
  if (c)
    dira |= mask
  else
    dira &= !mask

pri _drvl(pin) | mask
  mask := 1<<pin
  dira |= mask
  outa &= !mask

pri _drvh(pin) | mask
  mask := 1<<pin
  dira |= mask
  outa |= mask

pri _drvnot(pin) | mask
  mask := 1<<pin
  dira |= mask
  outa ^= mask

pri _drvw(pin, c) | mask
  mask := 1<<pin
  dira |= mask
  if (c)
    outa |= mask
  else
    outa &= !mask

' special case of _pinread where "pin" is a single pin
pri _pinr(pin)
  return (ina >> pin) & 1
    
pri _waitx(tim)
  asm
    add  tim, cnt
    waitcnt tim, #0
  endasm

pri _call(hubaddr)
  asm
    call hubaddr
  endasm

pri _ones(v) : r
  r := 0
  repeat while v <> 0
    if v & 1
      r++
    v := v >> 1
    
pri _getsec() : r = +long
  r := _getcnt()
  return r +/ __clkfreq_var

pri _getms() : r = +long | freq
  freq := __clkfreq_var +/ 1000
  r := _getcnt()
  return r +/ freq

pri _getus() : r = +long | freq
  freq := __clkfreq_var +/ 1000000
  r := _getcnt()
  return r +/ freq

''
'' _cogchk(id): check to see if cog id is still running
'' on P1 there is no instruction for this, so we use
'' coginit to start helpers until no more are left
'' returns -1 if running, 0 if not
dat
	org 0
_cogchk_helper
	rdlong	_cogchk_tmp, par wz
  if_z	jmp	#_cogchk_helper
  	cogid	_cogchk_tmp
	cogstop	_cogchk_tmp
_cogchk_tmp
	long	0

pri _cogchk(id) | flag, n
  flag := 0
  repeat
    n := _coginit(8, @_cogchk_helper, @flag)
  while n => 0 and n < id
  flag := 1 ' shut down all helpers
  ' if n is id, then the cog was free
  ' otherwise it is running
  return n <> id

''
'' bytefill/bytemove are here (in processor specific code)
'' because on P2 we can optimize them (long operations do
'' not have to be aligned)
''
pri bytefill(ptr, val, count)
  repeat count
    byte[ptr] := val
    ptr += 1
    
pri bytemove(dst, src, count) : origdst
  origdst := dst
  if (dst < src)
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

''
'' divide (n, nlo) by d, producing qlo and rlo (used in FRAC operation)
''
pri _div64(n, nlo, dlo) : qlo, rlo | q, r, d
  if dlo == 0
    qlo := -1
    rlo := -1
    return
    
  d := rlo := r := 0
  qlo := q := 0
  repeat 64
    asm
        ' Q <<= 1
        shl qlo, #1 wc
        rcl q, #1
        ' R <<= 1
        shl rlo, #1 wc	' r := r<<1
        rcl r, #1
	' bit 0 of r gets hi bit of n
	shl nlo, #1 wc
        rcl n, #1 wc		' bit 0 of r gets hi bit of n
        muxc rlo, #1
	
        cmp  rlo, dlo wc	' check for r <= d (r-d >= 0)
        cmpx r, d wc,wz
 if_b   jmp  #zz_skipfrac
        sub  rlo, dlo wc
        subx r, d
        or   qlo, #1
zz_skipfrac
   endasm
