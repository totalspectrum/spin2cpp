PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_swapab
	add	objptr, #4
	rdlong	_tmp001_, objptr
	sub	objptr, #4
	rdlong	_tmp002_, objptr
	wrlong	_tmp001_, objptr
	add	objptr, #4
	wrlong	_tmp002_, objptr
	sub	objptr, #4
_swapab_ret
	ret

_seq3
	mov	result2, arg1
	add	result2, #1
	mov	result3, arg1
	add	result3, #2
	mov	result1, arg1
_seq3_ret
	ret

_setit1
	mov	arg1, #1
	call	#_seq3
	wrlong	result1, objptr
	add	objptr, #4
	wrlong	result2, objptr
	add	objptr, #4
	wrlong	result3, objptr
	sub	objptr, #8
_setit1_ret
	ret

_setit2
	mov	arg1, #0
	call	#_seq3
	wrlong	result1, objptr
	add	objptr, #4
	wrlong	result2, objptr
	add	objptr, #4
	wrlong	result3, objptr
	sub	objptr, #8
_setit2_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
result2
	long	0
result3
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[3]
	org	COG_BSS_START
_tmp001_
	res	1
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
