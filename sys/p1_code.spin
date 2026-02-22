pri waitcnt(x)
  asm
    waitcnt x,#0
  endasm
pri _waitcnt(x)
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
  __clkfreq_ms := freq / 1000
  __clkfreq_us := freq / 1000000
  _bitcycles := 0
  asm
    clkset mode
  endasm

pri _reboot
  _clkset($80, 0)

pri _lockclr(id = long) | mask, rval
  mask := -1
  asm
    lockclr id wc
    muxc   rval,mask
  endasm
  return rval

pri _lockset(id = long) | mask, rval
  mask := -1
  asm
    lockset id wc
    muxc   rval,mask
  endasm
  return rval

' like lockset but has opposite return value
pri _locktry(id = long) | mask, rval
  mask := -1
  asm
    lockset id wc
    muxnc   rval,mask
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

pri _coginit(id = long, code, param) | parm
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

' not necessarily very accurate, but close enough
' for what we want to do with it (floating point)
pri _sqrt64(alo, ahi) | x0, x1
  if (ahi < 0)
    return 0

  ' get an initial estimate
  x0 := _sqrt(ahi)<<16
  x1 := (x0 + _div64(ahi, alo, x0)) >> 1
  return x1

con
 _rxpin = 31
 _txpin = 30

dat
_bitcycles long 0

pri _txraw(c) | val, nextcnt, bitcycles
  bitcycles := _bitcycles
  if (bitcycles == 0)
    bitcycles := _setbaud(__default_baud__)

  outa[_txpin] := 1
  dira[_txpin] := 1
  val := ((c | 256) << 1)
  nextcnt := cnt
  repeat 10
    _waitcnt(nextcnt += bitcycles)
    outa[_txpin] := val
    val >>= 1
  _waitcnt(nextcnt += bitcycles)
  outa[_txpin] := 1
  dira[_txpin] := 0  ' release DIR so other COGs can print
  return 1

' timeout is in 1024ths of a second (roughly milliseconds)
pri _rxraw(timeout = 0) | val, waitcycles, i, bitcycles
  bitcycles := _bitcycles
  if (bitcycles == 0)
    bitcycles := _setbaud(__default_baud__)
  dira[_rxpin] := 0

  if timeout
    waitcycles := cnt + timeout * (__clkfreq_var >> 10)   
    repeat
      if ina[_rxpin] == 0
        quit
      if cnt - waitcycles < 0
        return -1
  else
    repeat until ina[_rxpin] == 0
    
  waitcycles := cnt + (bitcycles>>1)
  val := 0
  repeat 8
    _waitcnt(waitcycles += bitcycles)
    val := (ina[_rxpin] << 7) | (val>>1)
  _waitcnt(waitcycles + bitcycles)
  return val

' like _rxraw, but no timeout and returns -1 immediately if no data
pri _rxpoll() | val, i, bitcycles, pin
  bitcycles := _bitcycles
  if (bitcycles == 0)
    bitcycles := _setbaud(__default_baud__)
  pin := _rxpin
  dira[pin] := 0

  if ina[pin] <> 0
     return -1
    
  waitcycles := cnt + (bitcycles>>1)
  val := 0
  repeat 8
    _waitcnt(waitcycles += bitcycles)
    val := (ina[pin] << 7) | (val>>1)
  _waitcnt(waitcycles + bitcycles)
  return val

pri _setbaud(rate) : r
  _bitcycles := r := __clkfreq_var / rate
  
pri _fltl(pin = long) | mask
  mask := 1<<pin
  dira &= !mask
  outa &= !mask
  
pri _flth(pin = long) | mask
  mask := 1<<pin
  dira &= !mask
  outa |= mask

pri _dirl(pin = long) | mask
  mask := 1<<pin
  dira &= !mask

pri _dirh(pin = long) | mask
  mask := 1<<pin
  dira |= mask

pri _dirnot(pin = long) | mask
  mask := 1<<pin
  dira ^= mask

pri _dirw(pin = long, c = long) | mask
  mask := 1<<pin
  if (c)
    dira |= mask
  else
    dira &= !mask

pri _drvl(pin = long) | mask
  mask := 1<<pin
  dira |= mask
  outa &= !mask

pri _drvh(pin = long) | mask
  mask := 1<<pin
  dira |= mask
  outa |= mask

pri _drvnot(pin = long) | mask
  mask := 1<<pin
  dira |= mask
  outa ^= mask

pri _drvw(pin = long, c = long) | mask
  mask := 1<<pin
  dira |= mask
  if (c)
    outa |= mask
  else
    outa &= !mask

' special case of _pinread where "pin" is a single pin
pri _pinr(pin = long)
  return (ina >> pin) & 1
    
pri _waitx(tim = long)
  asm
    add  tim, cnt
    waitcnt tim, #0
  endasm

pri _call(hubaddr)
  asm
    call hubaddr
  endasm

pri _ones(v = +long) : r = +long
  r := 0
  repeat while v <> 0
    if v & 1
      r++
    v := v >> 1
    
pri __builtin_parity(v = +long) : r = +long
  r := 0
  asm
    or   v, #0 wc
    muxc r, #1
  endasm

pri _getsec() : r = +long
  r := _getcnt()
  return r +/ __clkfreq_var

pri _getms() : r = +long | freq
  freq := __clkfreq_ms
  if freq == 0
    __clkfreq_ms := freq := __clkfreq_var +/ 1000
  r := _getcnt()
  return r +/ freq

pri _getus() : r = +long | freq
  freq := __clkfreq_us
  if freq == 0
    __clkfreq_us := freq := __clkfreq_var +/ 1000000
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
        rdlong  _cogchk_tmp, par wz
  if_z  jmp     #_cogchk_helper
        cogid   _cogchk_tmp
        cogstop _cogchk_tmp
_cogchk_tmp
        long    0

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
'' memmove/memset are here (in processor specific code)
'' because on P2 we can optimize them (long operations do
'' not have to be aligned)
''
pri {++specialfunc(memset)} __builtin_memset(ptr, val, count) : r
  r := ptr
  repeat count
    byte[ptr] := val
    ptr += 1

pri __builtin_longset(ptr, val, count) : r
  r := ptr
  repeat count
    long[ptr] := val
    ptr += 4
    
pri __builtin_memmove(dst, src, count) : origdst
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

pri longfill(ptr, val, count)
  __builtin_longset(ptr, val, count)
pri wordfill(ptr, val, count)
  repeat count
    word[ptr] := val
    ptr += 2
pri bytefill(ptr, val, count)
  __builtin_memset(ptr, val, count)

pri getregs(hubptr, cogptr, count)
  repeat while count > 0
    long[hubptr] := %reg[cogptr]
    hubptr += 4
    cogptr++
    --count

pri setregs(hubptr, cogptr, count)
  repeat while count > 0
    %reg[cogptr] := long[hubptr]
    hubptr += 4
    cogptr++
    --count

'' create a method pointer
'' for P1, addresses fit in 16 bits,
'' so the function pointer goes in the upper 16,
'' object pointer in lower 16

pri _make_methodptr(o, func) : ptr
  ptr := o | (func << 16)

pri _get_rawfuncaddr(o, func) : ptr
  ptr := func

'
' create a class interface from a definite object o and
' a skeleton list of functions skel containing n entries
'
pub _make_interfaceptrs(o, skel, n) : r | siz, i, p, f
  siz := n * 4
  p := _gc_alloc_managed(siz)
  if p == 0
    return p
  r := p
  repeat while n-- > 0
    f := long[skel]
    if f & 1
      long[p] := r | (f & -2)
    else
      long[p] := o | f
    p += 4
    skel += 4


''
'' divide (n, nlo) by d, producing qlo and rlo (used in FRAC operation)
''
pri _div64(n, nlo, dlo) : qlo, rlo | q, r, d
  if dlo == 0
    qlo := -1
    rlo := -1
    return
  if n == 0
    qlo := nlo +/ dlo
    rlo := nlo +// dlo
    return
  d := rlo := r := 0
  qlo := q := 0
  repeat 64
    asm
        ' Q <<= 1
        shl qlo, #1 wc
        rcl q, #1
        ' R <<= 1
        shl rlo, #1 wc  ' r := r<<1
        rcl r, #1
        ' bit 0 of r gets hi bit of n
        shl nlo, #1 wc
        rcl n, #1 wc            ' bit 0 of r gets hi bit of n
        muxc rlo, #1
        
        cmp  rlo, dlo wc,wz     ' check for r <= d (r-d >= 0)
        cmpx r, d wc,wz
 if_b   jmp  #zz_skipfrac
        sub  rlo, dlo wc
        subx r, d
        or   qlo, #1
zz_skipfrac
   endasm

pri _muldiv64(mult1, mult2, divisor) : r | mlo, mhi
  mlo := mult1 * mult2
  mhi := mult1 +** mult2
  r := _div64(mhi, mlo, divisor)

pri __builtin_bswap16(x = +long) : y = +long
  y := 0
  y.byte[0] := x.byte[1]
  y.byte[1] := x.byte[0]

pri __builtin_bswap32(x = +long) : y = +long
  y.byte[0] := x.byte[3]
  y.byte[1] := x.byte[2]
  y.byte[2] := x.byte[1]
  y.byte[3] := x.byte[0]

pri __builtin_movbyts(v = +long, m = +long) : r = +long
  r := v.byte[m&3]
  r.byte[1] := v.byte[(m>>2)&3]
  r.byte[2] := v.byte[(m>>4)&3]
  r.byte[3] := v.byte[(m>>6)&3]

pri __builtin_mergew(v = +long) : r = +long
  r := __builtin_bit_permute_step(v,$0000AAAA,15)
  r := __builtin_bit_permute_step(r,$0000CCCC,14)
  r := __builtin_bit_permute_step(r,$0000F0F0,12)
  r := __builtin_bit_permute_step(r,$0000FF00, 8)

pri __builtin_splitw(v = +long) : r = +long
  r := __builtin_bit_permute_step(v,$22222222, 1)
  r := __builtin_bit_permute_step(r,$0C0C0C0C, 2)
  r := __builtin_bit_permute_step(r,$00F000F0, 4)
  r := __builtin_bit_permute_step(r,$0000FF00, 8)

pri __builtin_mergeb(v = +long) : r = +long
  r := __builtin_bit_permute_step(v,$00AA00AA, 7)
  r := __builtin_bit_permute_step(r,$0000CCCC,14)
  r := __builtin_bit_permute_step(r,$00F000F0, 4)
  r := __builtin_bit_permute_step(r,$0000FF00, 8)

pri __builtin_splitb(v = +long) : r = +long
  r := __builtin_bit_permute_step(v,$0A0A0A0A, 3)
  r := __builtin_bit_permute_step(r,$00CC00CC, 6)
  r := __builtin_bit_permute_step(r,$0000F0F0,12)
  r := __builtin_bit_permute_step(r,$0000FF00, 8)

pri __builtin_seussf(x = +long) : r = +long
return ( ((x & $0200_0002) <<  4) {
}      | ((x >> 25) & $0000_0010) {
}      | ((x & $4180_8000) rol  8) {
}      | ((x & $0000_0401) << 11) {
}      | ((x >> 20) & $0000_0002) {
}      | ((x & $0000_0860) << 14) {
}      | ((x >> 17) & $0000_0404) {
}      | ((x & $0000_0004) << 16) {
}      | ((x & $0000_0200) << 17) {
}      | ((x >> 14) & $0000_4000) {
}      | ((x >> 13) & $0000_0200) {
}      | ((x & $0000_0100) << 20) {
}      | ((x & $0000_0008) << 21) {
}      | ((x & $8400_1090) rol  23) {
}      | ((x >>  7) & $0000_0080) {
}      | ((x >>  5) & $0000_9100) {
}      | ((x >>  3) & $0000_2000) {
}      | ((x >>  2) & $0001_0000) {
}      ) ^ $354D_AE51

pri __builtin_seussr(x = +long) : r = +long
return ( ((x & $0001_0000) <<  2) {
}      | ((x & $0000_2000) <<  3) {
}      | ((x & $0000_9100) <<  5) {
}      | ((x & $0000_0080) <<  7) {
}      | ((x & $4842_0008) rol  9) {
}      | ((x >> 21) & $0000_0008) {
}      | ((x >> 20) & $0000_0100) {
}      | ((x & $0000_0200) << 13) {
}      | ((x & $0000_4000) << 14) {
}      | ((x >> 17) & $0000_0200) {
}      | ((x >> 16) & $0000_0004) {
}      | ((x & $0000_0404) << 17) {
}      | ((x >> 14) & $0000_0860) {
}      | ((x & $0000_0002) << 20) {
}      | ((x >> 11) & $0000_0401) {
}      | ((x & $8080_0041) rol  24) {
}      | ((x & $0000_0010) << 25) {
}      | ((x >>  4) & $0200_0002) {
}      ) ^ $EB55_032D

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

