pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_getptr
	add	objptr, #16
	mov	result1, objptr
	sub	objptr, #16
_getptr_ret
	ret

__lockreg
	long	0
objptr
	long	@@@objmem
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[8]
	org	COG_BSS_START
	fit	496
