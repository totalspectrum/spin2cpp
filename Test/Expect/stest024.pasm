DAT
	org	0

_test1
	mov	_test1_x, arg1_ wz
 if_e	jmp	#L_032_
	cmps	arg2_, #0 wz
 if_ne	mov	result_, arg3_
 if_ne	jmp	#_test1_ret
L_032_
	neg	result_, #1
_test1_ret
	ret

_test1_x
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
