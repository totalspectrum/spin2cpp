PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_test1
	mov	_var_01, arg1 wz
 if_ne	jmp	#L__0002
	cmps	arg2, #0 wz
 if_e	jmp	#L__0001
L__0002
	mov	result1, arg3
	jmp	#_test1_ret
L__0001
	neg	result1, #1
_test1_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var_01
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
