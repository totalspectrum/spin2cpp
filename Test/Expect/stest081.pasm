pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_test
	mov	_var01, arg01
	or	_var01, arg02
	wrlong	_var01, objptr
	or	_var01, arg03
	add	objptr, #4
	wrlong	_var01, objptr
	add	arg01, #1
	or	arg01, arg02
	add	objptr, #4
	wrlong	arg01, objptr
	or	arg01, arg03
	add	objptr, #4
	wrlong	arg01, objptr
	sub	objptr, #12
_test_ret
	ret

__lockreg
	long	0
objptr
	long	@@@objmem
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
objmem
	long	0[4]
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496
