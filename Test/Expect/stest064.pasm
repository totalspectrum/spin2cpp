PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_getptr
	add	objptr, #16
	mov	result1, objptr
	sub	objptr, #16
_getptr_ret
	ret

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
	res	8
	org	COG_BSS_START
	fit	496
