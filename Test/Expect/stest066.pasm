pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_set
	add	objptr, #4
	rdlong	_var01, objptr
	add	objptr, #4
	add	_var01, objptr
	wrbyte	arg01, _var01
	sub	objptr, #8
_set_ret
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
	long	0[3]
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
