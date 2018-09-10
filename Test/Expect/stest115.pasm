PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_shalf
	rdlong	result1, objptr
	sar	result1, #1
_shalf_ret
	ret

_uhalf
	add	objptr, #4
	rdlong	result1, objptr
	sub	objptr, #4
	shr	result1, #1
_uhalf_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[2]
	org	COG_BSS_START
	fit	496
