pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_func
	add	objptr, #8
	rdlong	_var_00, objptr
	add	objptr, #4
	rdbyte	_tmp002_, objptr
	add	_var_00, _tmp002_
	sub	objptr, #8
	wrlong	_var_00, objptr
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
_var_00
	res	1
	fit	496
