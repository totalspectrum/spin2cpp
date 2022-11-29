pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fetchx
	mov	result1, arg01
	shl	result1, #1
	add	result1, arg01
	shl	result1, #3
	add	result1, objptr
	shl	arg02, #2
	add	arg02, result1
	rdlong	result1, arg02
_fetchx_ret
	ret

_fetchy
	mov	result1, arg01
	shl	result1, #1
	add	result1, arg01
	shl	result1, #3
	add	objptr, #96
	add	result1, objptr
	shl	arg02, #2
	add	arg02, result1
	rdlong	result1, arg02
	sub	objptr, #96
_fetchy_ret
	ret

_fetchz
	mov	result1, arg01
	shl	result1, #1
	add	result1, arg01
	shl	result1, #3
	add	objptr, #192
	add	result1, objptr
	shl	arg02, #2
	add	arg02, result1
	rdlong	result1, arg02
	sub	objptr, #192
_fetchz_ret
	ret

objptr
	long	@@@objmem
result1
	long	0
COG_BSS_START
	fit	496
objmem
	long	0[72]
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496
