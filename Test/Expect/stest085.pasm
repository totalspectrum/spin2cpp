PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_setup
	shl	arg1, #2
	add	arg1, objptr
	add	objptr, #40
	wrlong	arg1, objptr
	add	objptr, #4
	wrlong	arg1, objptr
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
	long	0[12]
	org	COG_BSS_START
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496
