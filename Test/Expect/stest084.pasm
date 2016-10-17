PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_update
	wrlong	arg1, objptr
	add	objptr, #4
	wrlong	arg2, objptr
	sub	objptr, #4
_update_ret
	ret

_bump
	rdlong	arg2, objptr
	add	objptr, #4
	rdlong	bump_tmp002_, objptr
	sub	objptr, #4
	add	arg2, bump_tmp002_
	mov	arg1, arg2
	call	#_update
	rdlong	arg2, objptr
	add	objptr, #4
	rdlong	bump_tmp002_, objptr
	sub	objptr, #4
	add	arg2, bump_tmp002_
	mov	arg1, arg2
	call	#_update
_bump_ret
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
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
bump_tmp002_
	res	1
	fit	496
