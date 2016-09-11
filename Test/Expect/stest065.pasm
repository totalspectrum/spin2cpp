PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_set
	add	objptr, #9
	rdbyte	_tmp001_, objptr
	sub	objptr, #5
	wrlong	_tmp001_, objptr
	sub	objptr, #4
_set_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	res	3
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
