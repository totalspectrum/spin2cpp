''
'' ROM bytecode specific functions
''

dat
__helper_cog long -1
__helper_cmd long 0
__helper_arg long 0[4]

        org 0
__helper_entry
        mov	:cmdptr, par
	mov	dira, :dira_init
	mov	outa, :outa_init
	mov	:nextcycle, cnt
	add	:nextcycle, :delay
:loop
	waitcnt	:nextcycle, #0
	add	:nextcycle, :delay
	xor	outa, :outa_toggle
	jmp	#:loop
	
:cmdptr
        long 0
:dira_init
:outa_init
        long (1<<31) | (1<<26)
:outa_toggle
	long (1<<26)
:nextcycle
	long 0
:delay
	long 20_000_000
__helper_done

''
'' init helper code
''
pri __init__ | cog
  cog := __helper_cog
  if cog => 0
    return
  cog := cognew(@__helper_entry, @__helper_cmd)

' FIXME needs to run in helper
pri {++needsinit} _txraw(c)
  return

' timeout is in 1024ths of a second (roughly milliseconds)
' FIXME needs to run in helper
pri {++needsinit} _rxraw(timeout = 0)
  return -1
  
''
'' divide (n, nlo) by d, producing qlo and rlo (used in FRAC operation)
'' FIXME: needs to run in helper
''
pri {++needsinit} _div64(n, nlo, dlo) : qlo, rlo | q, r, d
  qlo := -1
  rlo := -1
  return

pri _waitx(tim)
  tim += cnt
  waitcnt(tim)

pri _getcnt : r = +long
  r := cnt

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
    n := coginit(8, @_cogchk_helper, @flag)
  while n => 0 and n < id
  flag := 1 ' shut down all helpers
  ' if n is id, then the cog was free
  ' otherwise it is running
  return n <> id

