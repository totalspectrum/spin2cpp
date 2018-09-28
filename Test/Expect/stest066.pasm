PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_set
	add	objptr, #4
	rdlong	_var_01, objptr
	add	objptr, #4
	add	_var_01, objptr
	wrbyte	arg1, _var_01
	sub	objptr, #8
_set_ret
	ret

objptr
	long	@@@objmem
COG_BSS_START
	fit	496
objmem
	long	0[3]
	org	COG_BSS_START
_var_01
	res	1
arg1
	res	1
	fit	496
