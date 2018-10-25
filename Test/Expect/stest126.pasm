PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_mystrlen
	mov	_var_02, arg1
LR__0001
	rdbyte	_tmp001_, _var_02 wz
 if_ne	add	_var_02, #1
 if_ne	jmp	#LR__0001
	sub	_var_02, arg1
	mov	result1, _var_02
_mystrlen_ret
	ret

_wordxpand
LR__0002
	rdword	_var_02, arg2
	shl	_var_02, #16
	shr	_var_02, #16 wz
	wrlong	_var_02, arg1
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
_tmp001_
	res	1
_var_02
	res	1
arg1
	res	1
arg2
	res	1
	fit	496
