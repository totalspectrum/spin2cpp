pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_strlen
	neg	_var01, #1
LR__0001
	rdbyte	_var02, arg01 wz
	add	arg01, #1
	add	_var01, #1
 if_ne	jmp	#LR__0001
	mov	result1, _var01
_strlen_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg01
	res	1
	fit	496
