PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_test
	mov	_var_05, arg1
	or	_var_05, arg2
	wrlong	_var_05, objptr
	or	_var_05, arg3
	add	objptr, #4
	wrlong	_var_05, objptr
	add	arg1, #1
	or	arg1, arg2
	add	objptr, #4
	wrlong	arg1, objptr
	or	arg1, arg3
	add	objptr, #4
	wrlong	arg1, objptr
	sub	objptr, #12
_test_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[4]
	org	COG_BSS_START
_var_05
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
