pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_mystrlen
	mov	_var01, arg1
LR__0001
	rdbyte	_var02, _var01 wz
 if_ne	add	_var01, #1
 if_ne	jmp	#LR__0001
	sub	_var01, arg1
	mov	result1, _var01
_mystrlen_ret
	ret

_wordxpand
LR__0002
	rdword	_var01, arg2
	shl	_var01, #16
	shr	_var01, #16 wz
	wrlong	_var01, arg1
	add	arg1, #4
	add	arg2, #2
 if_ne	jmp	#LR__0002
_wordxpand_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
