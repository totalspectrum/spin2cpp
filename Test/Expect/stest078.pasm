PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_setout
	rdlong	_tmp002_, objptr
	or	DIRA, _tmp002_
	or	OUTA, _tmp002_
_setout_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	res	1
	org	COG_BSS_START
_tmp002_
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
