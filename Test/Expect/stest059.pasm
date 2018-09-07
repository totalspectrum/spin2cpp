PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_func
	add	objptr, #8
	rdlong	_var_01, objptr
	add	objptr, #4
	rdbyte	_tmp002_, objptr
	add	_var_01, _tmp002_
	sub	objptr, #8
	wrlong	_var_01, objptr
	sub	objptr, #4
_func_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[4]
	org	COG_BSS_START
_tmp002_
	res	1
_var_01
	res	1
	fit	496
