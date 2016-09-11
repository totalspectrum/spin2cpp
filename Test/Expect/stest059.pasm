PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_func
	add	objptr, #8
	rdlong	_tmp001_, objptr
	add	objptr, #4
	rdbyte	_tmp002_, objptr
	add	_tmp001_, _tmp002_
	sub	objptr, #8
	wrlong	_tmp001_, objptr
	sub	objptr, #4
_func_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	res	4
	org	COG_BSS_START
_tmp001_
	res	1
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
