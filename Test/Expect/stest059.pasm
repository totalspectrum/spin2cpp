pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_func
	add	objptr, #8
	rdlong	_var01, objptr
	add	objptr, #4
	rdbyte	_var02, objptr
	add	_var01, _var02
	sub	objptr, #8
	wrlong	_var01, objptr
	sub	objptr, #4
_func_ret
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
	long	0[4]
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
	fit	496
