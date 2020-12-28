pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main
	mov	result2, #2
	mov	result1, #1
	wrlong	result1, objptr
	add	objptr, #4
	wrlong	result2, objptr
	sub	objptr, #4
_main_ret
	ret

__lockreg
	long	0
objptr
	long	@@@objmem
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
result2
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[2]
	org	COG_BSS_START
	fit	496
