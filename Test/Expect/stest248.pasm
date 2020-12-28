pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	_var01, #1
	add	objptr, #4
	wrlong	_var01, objptr
	sub	objptr, #4
_demo_ret
	ret

_arrobj_info
	shl	arg01, #2
	add	arg01, objptr
	rdlong	result1, arg01
_arrobj_info_ret
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
	long	0[10]
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
	fit	496
