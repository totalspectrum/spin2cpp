PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_setup
	mov	_tmp001_, objptr
	shl	arg1, #2
	add	_tmp001_, arg1
	add	objptr, #40
	wrlong	_tmp001_, objptr
	add	objptr, #4
	wrlong	_tmp001_, objptr
	sub	objptr, #44
_setup_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	res	12
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
