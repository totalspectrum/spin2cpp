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
:loop
	rdlong	:cmd, :cmdptr wz
  if_z	jmp	#:loop
  	add	:cmdptr, #4
	rdlong	:arg0, :cmdptr
	add	:cmdptr, #4
	rdlong	:arg1, :cmdptr
	add	:cmdptr, #4
	rdlong	:arg2, :cmdptr
	sub	:cmdptr, #4

	' commands:
	' #1 == _setbaud
	' #2 == _txraw
	' #3 == _rxraw
	' #4 == _div64
	add	:cmd, #:cmdtable
	jmp	:cmd
:cmddone
	' write back results (at most 2 results)
	wrlong	:arg1, :cmdptr
	sub	:cmdptr, #4
	wrlong	:arg0, :cmdptr
	sub	:cmdptr, #4
	mov	:cmd, #0
	wrlong	:cmd, :cmdptr
	jmp	#:loop

:setbaud
	mov	:baud_delay, :arg0
	jmp	#:cmddone
:txraw
	or	outa, :txmask
	or	dira, :txmask
	or	:arg0, #$100
	shl	:arg0, #1
	mov	:nextcycle, cnt
	add	:nextcycle, :baud_delay
	mov	:arg2, #10
:txloop
	waitcnt :nextcycle, :baud_delay
	shr	:arg0, #1 wc
  	muxc	outa, :txmask
  	djnz	:arg2, #:txloop
	jmp	#:cmddone
	
:cmd    long 0
:arg0	long 0
:arg1	long 0
:arg2	long 0

:cmdtable
	jmp	#:cmddone	' 0 == nop
	jmp	#:setbaud	' 1 == setbaud
	jmp	#:txraw		' 2 == txraw
	
:baud_delay
	long 80_000_000 / 115_200

:dira_init
:outa_init
        long (1<<30)
:txmask
	long (1<<30)
:rxmask
	long (1<<31)
	
:nextcycle
	long 0
:cmdptr long 0

__helper_done

''
'' init helper code
''
pri __init__ | cog
  cog := __helper_cog
  if cog => 0
    return
  cog := cognew(@__helper_entry, @__helper_cmd)

pri {++needsinit} _remotecall(cmd, arg0 = 0, arg1 = 0, arg2 = 0)
  longmove(@__helper_arg[0], @arg0, 3)
  __helper_cmd := cmd
  repeat until __helper_cmd == 0
  
pri _setbaud(rate)
  _remotecall(1, __clkfreq_var / rate)
  
' FIXME needs to run in helper
pri _txraw(c)
  _remotecall(2, c)
  return 1


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

