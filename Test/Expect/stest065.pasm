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

_tmp001_
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	res	3
	org	COG_BSS_START
	fit	496
