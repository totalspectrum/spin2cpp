PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_func
	add	objptr, #8
	rdlong	result1, objptr
	add	objptr, #4
	rdbyte	_tmp002_, objptr
	sub	objptr, #12
	add	result1, _tmp002_
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
