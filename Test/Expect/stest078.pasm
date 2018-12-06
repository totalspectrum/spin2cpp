pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_setout
	rdlong	_var01, objptr
	or	dira, _var01
	or	outa, _var01
_setout_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[1]
	org	COG_BSS_START
_var01
	res	1
	fit	496
