PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_test1
	mov	_var_01, arg1 wz
	mov	_var_02, arg2
	mov	_var_03, arg3
 if_ne	jmp	#L__0002
	cmps	_var_02, #0 wz
 if_ne	jmp	#L__0002
 if_e	jmp	#L__0001
L__0002
	mov	result1, _var_03
	jmp	#_test1_ret
L__0001
	neg	result1, #1
_test1_ret
	ret

_var_01
	long	0
_var_02
	long	0
_var_03
	long	0
arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496
