PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_setout
	rdlong	_tmp001_, objptr
	or	DIRA, _tmp001_
	or	OUTA, _tmp001_
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
_tmp001_
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
