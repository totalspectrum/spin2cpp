DAT
	org	0

_test1
	mov	_test1_x, arg1 wz
	mov	_test1_y, arg2
	mov	_test1_z, arg3
 if_ne	jmp	#L_033_
	cmps	_test1_y, #0 wz
 if_ne	jmp	#L_033_
 if_e	jmp	#L_032_
L_033_
	mov	result1, _test1_z
	jmp	#_test1_ret
L_032_
	neg	result1, #1
_test1_ret
	ret

_test1_x
	long	0
_test1_y
	long	0
_test1_z
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
	fit	496
