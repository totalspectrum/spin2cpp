PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_strlen
	neg	_var_02, #1
L__0002
	rdbyte	_var_03, arg1 wz
	add	arg1, #1
	add	_var_02, #1
 if_ne	jmp	#L__0002
	mov	result1, _var_02
_strlen_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_02
	res	1
_var_03
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
