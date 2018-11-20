pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_setout
	rdlong	_tmp002_, objptr
	or	dira, _tmp002_
	or	outa, _tmp002_
_setout_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[1]
	org	COG_BSS_START
_tmp002_
	res	1
	fit	496
