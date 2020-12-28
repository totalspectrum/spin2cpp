pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_incby
	shl	arg01, #2
	add	arg01, objptr
	rdlong	_var01, arg01
	add	_var01, arg02
	wrlong	_var01, arg01
_incby_ret
	ret

__lockreg
	long	0
objptr
	long	@@@objmem
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
objmem
	long	0[10]
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
arg02
	res	1
	fit	496
