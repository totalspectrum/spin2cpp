''
'' ROM bytecode specific functions
''

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
    n := coginit(8, @_cogchk_helper, @flag)
  while n => 0 and n < id
  flag := 1 ' shut down all helpers
  ' if n is id, then the cog was free
  ' otherwise it is running
  return n <> id

'
' helper cog
' this helper runs in any cog and handles serial I/O and some other
' auxiliary functions. It works by polling the __helper_cmd
' mailbox for commands. If more than a second passes without any
' command being received, it checks to see if it is the only cog
' still running, and if so, it terminates itself.
'

dat
__helper_cmd long 0
__helper_arg long 0[4]

__heap_base  word $ffff
__helper_cog byte $ff
__lockreg    byte $ff

        org 0
__helper_entry
        mov     :cmdptr, par
:loop
        rdlong  :timeout, #0            ' fetch clock frequency from HUB 0
        shr     :timeout, #5            ' loop takes around 16 cycles, check every half second
:waitcmd
        rdlong  :cmd, :cmdptr wz
  if_nz jmp     #:docmd
        djnz    :timeout, #:waitcmd
        
        '
        ' if we get here, there have been no commands for a while
        ' check to see if we are the only cog left alive
        '
        call    #:check_all_cogs
        cmp     :cog_count, #1 wz
  if_nz jmp     #:loop
        ' we are the only cog left, halt
        cogid    :cog_count
        cogstop  :cog_count
        
:docmd
        add     :cmdptr, #4
        rdlong  :arg0, :cmdptr
        add     :cmdptr, #4
        rdlong  :arg1, :cmdptr
        add     :cmdptr, #4
        rdlong  :arg2, :cmdptr
        sub     :cmdptr, #4

        ' commands:
        ' #1 == _setbaud
        ' #2 == _txraw
        ' #3 == _rxraw
        ' #4 == _div64
        add     :cmd, #:cmdtable
        jmp     :cmd
:cmddone
        ' write back results (at most 2 results)
        wrlong  :arg1, :cmdptr
        sub     :cmdptr, #4
        wrlong  :arg0, :cmdptr
        sub     :cmdptr, #4
        mov     :cmd, #0
        wrlong  :cmd, :cmdptr
        jmp     #:loop

:setbaud
        mov     :baud_delay, :arg0
        jmp     #:cmddone

:txraw
        or      outa, :txmask
        or      dira, :txmask
        or      :arg0, #$100
        shl     :arg0, #1
        mov     :nextcycle, cnt
        add     :nextcycle, :baud_delay
        mov     :arg2, #10
:txloop
        waitcnt :nextcycle, :baud_delay
        shr     :arg0, #1 wc
        muxc    outa, :txmask
        djnz    :arg2, #:txloop
        jmp     #:cmddone

:rxraw
        andn    dira, :rxmask
        
        cmp     :arg0, #0 wz
  if_z  jmp     #:rxnowait
        add     :arg0, cnt      ' final timeout time
:rxwait
        mov     :r, ina
        test    :r, :rxmask wz
  if_z  jmp     #:do_rx
        mov     :arg1, :arg0    ' timeout wait cycle
        sub     :arg1, cnt wc   ' check count
  if_ae jmp     #:rxwait
        neg     :arg0, #1       ' return -1
        jmp     #:cmddone
:rxnowait
        mov     :r, ina
        test    :r, :rxmask wz
  if_nz jmp     #:rxnowait
  
:do_rx
        mov     :arg0, #0
        mov     :arg1, :baud_delay
        shr     :arg1, #1
        add     :arg1, cnt
        mov     :counter, #8
:rxloop
        add     :arg1, :baud_delay
        waitcnt :arg1, #0
        shr     :arg0, #1
        mov     :r, ina
        test    :r, :rxmask wz
  if_nz or      :arg0, #1<<7
        djnz    :counter, #:rxloop

        add     :arg1, :baud_delay
        waitcnt :arg1, #0

        jmp     #:cmddone

'
' divide 64 bit unsigned by 32 bit unsigned
'
:div64
        mov     :n, :arg0
        mov     :nlo, :arg1
        mov     :dlo, :arg2

        mov     :r, #0
        mov     :rlo, #0
        mov     :q, #0
        mov     :qlo, #0
        mov     :counter, #64

        ' FIXME: could optimize case of :n == 0
:divloop
        ' Q <<= 1
        shl :qlo, #1 wc
        rcl :q, #1
        ' R <<= 1
        shl :rlo, #1 wc ' r := r<<1
        rcl :r, #1
        ' bit 0 of r gets hi bit of n
        shl :nlo, #1 wc
        rcl :n, #1 wc           ' bit 0 of r gets hi bit of n
        muxc :rlo, #1
        
        cmp  :rlo, :dlo wc,wz   ' check for r <= d (r-d >= 0)
        cmpx :r, #0 wc,wz
  if_b  jmp  #:skipset
        sub  :rlo, :dlo wc
        subx :r, #0
        or   :qlo, #1
:skipset
        djnz    :counter, #:divloop
        
        mov     :arg0, :qlo
        mov     :arg1, :rlo

        jmp     #:cmddone
        
        
'
' function to count number of cogs running
' works by starting as many cogs as we can with a helper
'
:check_all_cogs
        mov     :cogchk_arg, :flag_ptr  ' parameter for call
        shl     :cogchk_arg, #14
        or      :cogchk_arg, :helper_ptr ' code for call
        shl     :cogchk_arg, #2
        or      :cogchk_arg, #$f        ' start any cog
        
        mov     :arg0, #0
        wrlong  :arg0, :flag_ptr
        mov     :cog_count, #8          ' assume all cogs free (this will be false :))
:check_loop
        coginit :cogchk_arg wc, nr      ' don't care which cog we started, just whether it did start
  if_nc sub     :cog_count, #1
  if_nc jmp     #:check_loop
        wrlong  :cog_count, :flag_ptr   ' shut down extra cogs (cog_count will be at least 1!)
:check_all_cogs_ret
        ret

        ' coginit argument
:cogchk_arg
        long 0

        ' 
:cmd    long 0
:arg0   long 0
:arg1   long 0
:arg2   long 0
:arg3   long 0

:cmdtable
        jmp     #:cmddone       ' 0 == nop
        jmp     #:setbaud       ' 1 == setbaud
        jmp     #:txraw         ' 2 == txraw
        jmp     #:rxraw         ' 3 == rxraw
        jmp     #:div64         ' 4 == unsigned 64 / 32 division
        
:baud_delay
        long 80_000_000 / 115_200

:timeout
        long 80_000_000

:dira_init
        long (1<<30)
:outa_init
        long -1
:txmask
        long (1<<30)
:rxmask
        long (1<<31)
        
:nextcycle
        long 0
:cmdptr long 0
:cog_count long 0
:helper_ptr
        long @@@_cogchk_helper
:flag_ptr
        long @@@__helper_arg

        ' temporary variables
:n      res 1
:nlo    res 1
:dlo    res 1
:d      res 1
:qlo    res 1
:q      res 1
:rlo    res 1
:r      res 1
:counter res 1

__helper_done

''
'' init helper code
''
pri __init__ | cog
  __clkfreq_ms := __clkfreq_var / 1000
  __clkfreq_us := __clkfreq_var / 1000000
  cog := __helper_cog
  if cog <> $ff
    return
  cog := cognew(@__helper_entry, @__helper_cmd)
  ' vbase := word[8]
  ' size := word[10] - vbase
  ' bytefill(vbase, 0, size)
  
pri {++needsinit} _remotecall(cmd, arg0 = 0, arg1 = 0, arg2 = 0) | rlock
  rlock := __getlockreg
  repeat while _lockset(rlock)
  longmove(@__helper_arg[0], @arg0, 3)
  __helper_cmd := cmd
  repeat until __helper_cmd == 0
  _lockclr(rlock)
  
pri _setbaud(rate)
  _remotecall(1, __clkfreq_var / rate)
  
pri _txraw(c)
  _remotecall(2, c)
  return 1


' timeout is in 1024ths of a second (roughly milliseconds)
' FIXME needs to run in helper
pri _rxraw(timeout = 0)
  if timeout
    timeout *= __clkfreq_var >> 10
  _remotecall(3, timeout)
  return __helper_arg[0]

''
'' divide (n, nlo) by d, producing qlo and rlo (used in FRAC operation)
''
pri _div64(n, nlo, dlo) : qlo, rlo | q, r, d
  _remotecall(4, n, nlo, dlo)
  qlo := __helper_arg[0]
  rlo := __helper_arg[1]

pri _unsigned_div(n, d) : r
  _remotecall(4, 0, n, d)
  return __helper_arg[0]

pri _unsigned_mod(n, d)
  _remotecall(4, 0, n, d)
  return __helper_arg[1]

pri _unsigned_himul(a, b) : r
  r := a**b
  if a < 0
    r += b
  if b < 0
    r += a

pri _waitx(tim = long)
  tim += cnt
  waitcnt(tim)

pri _getcnt : r = +long
  r := cnt

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
    
pri _ones(v = +long) : r = +long
  repeat
    r += v&1
  while v>>=1

pri __builtin_parity(v = +long) : r = +long
  return _ones(v) & 1

' find 64 bit integer square root (approximate)
' not necessarily very accurate, but close enough
' for what we want to do with it (floating point)
pri _sqrt64(alo, ahi) | x0, x1
  if (ahi < 0)
    return 0

  ' get an initial estimate of sqrt
  x0 := (^^ahi)<<16
  ' do one round of iteration
  x1 := (x0 + _div64(ahi, alo, x0)) >> 1
  return x1

pri _getsec() : r = +long
  r := _getcnt()
  return r +/ __clkfreq_var

pri {++needsinit} _getms() : r = +long | freq
  freq := __clkfreq_ms
  r := _getcnt()
  return r +/ freq

pri {++needsinit} _getus() : r = +long | freq
  freq := __clkfreq_us
  r := _getcnt()
  return r +/ freq

pri __topofstack(ptr)
  return @ptr

{
''
'' unsigned comparison: return sign of a-b where a and b are unsigned
''
pri _unsigned_cmp(a, b)
  if (a => 0)
    if (b => 0)
      return a - b
    else
      ' b appears negative, a does not
      ' then a < b, so return -1
      return -1
  else
    ' a appears negative
    if b => 0
      ' b does not appear negative
      ' so a-b should appear positive
      return 1
  return b-a
}

pri __builtin_strcpy(dst, src) : r=@byte | n
  n := __builtin_strlen(src)+1
  bytemove(dst, src, n)
  return dst

pri __getlockreg : r
  r := __lockreg
  if r == $ff
    __lockreg := r := _locknew

pri __get_heap_base : r
  r := __heap_base

pri _cogid : r
  r := __interp_cogid

pri __gosub_helper(pc, vbase, pbase)
  __interp_vbase := vbase
  __interp_pbase := pbase
  __interp_pcurr := pc  ' jump to new address

pri _make_methodptr(o, func) | ptr
  ptr := _gc_alloc_managed(8)
  if (ptr)
    long[ptr] := o
    long[ptr+4] := func
  return ptr

'
' up to 8 parameters works OK; more than that and we could
' run into problems
'
pri __call_methodptr
  __interp_vbase := long[OUTB]
  result := word[OUTB+6]<<1  ' function offset as words
  __interp_pbase := word[OUTB+4]       ' new pbase
  __interp_dcurr += word[__interp_pbase][result+1]
  __interp_pcurr := word[__interp_pbase][result~] + __interp_pbase ' new pc

'
' memset(): we may want to optimize this to use longfill in special cases?
'
pri __builtin_memset(ptr, val, count) : r
  r := ptr
  bytefill(ptr, val, count)

pri _lockmem(addr) | oldlock, oldmem, lockreg
  lockreg := __getlockreg
  repeat
    repeat
      oldlock := _lockset(lockreg)
    while oldlock
    oldmem := byte[addr]
    if oldmem == 0
      long[addr] := 1
    _lockclr(lockreg)
  while oldmem <> 0

pri _unlockmem(addr) | oldlock
  long[addr] := 0
    
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
  ' the minus sign is because in Spin TRUE is -1
  rhi := (ahi + bhi) - ((rlo := alo + blo) +< blo)

pri _int64_sub(alo, ahi, blo, bhi) : rlo, rhi
  ' negate blo, bhi
  blo := !blo + 1
  bhi := !bhi - (blo == 0)
  ' now add them
  rlo, rhi := _int64_add(alo, ahi, blo, bhi)

' compare unsigned alo, ahi, return -1, 0, or +1
pri _int64_cmpu(alo, ahi, blo, bhi) : r
  if ahi == bhi
    if alo +< blo
      return -1
    return -(alo <> blo)

  if ahi +< bhi
    return -1
  return 1

' compare signed alo, ahi, return -1, 0, or +1
pri _int64_cmps(alo, ahi, blo, bhi) : r | s
  alo, ahi := _int64_sub(alo, ahi, blo, bhi)
  r := (ahi ~> 31)
  if (alo)
    r |= 1

  
