DAT
	org	0

_test1
	mov	_test1_x, arg1_ wz
	mov	_test1_y, arg2_
	mov	_test1_z, arg3_
 if_ne	jmp	#L_033_
	cmps	_test1_y, #0 wz
 if_ne	jmp	#L_033_
 if_e	jmp	#L_032_
L_033_
	mov	result_, _test1_z
	jmp	#_test1_ret
L_032_
	neg	result_, #1
_test1_ret
	ret

_test1_x
	long	0
_test1_y
	long	0
_test1_z
	long	0
arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
	long	0
result_
	long	0
