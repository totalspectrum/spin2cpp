pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_ident
	mov	result1, arg01
	mov	_var02, arg02
	mov	result2, _var02
_ident_ret
	ret

_test
	mov	result1, #0
	mov	_ident_x_01, #0
	mov	result2, _ident_x_01
_test_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
result2
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_ident_x
	res	1
_ident_x_01
	res	1
_var01
	res	1
_var02
	res	1
arg01
	res	1
arg02
	res	1
	fit	496
