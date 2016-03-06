DAT
	org	0

test1
	mov	test1_x_, arg1_ wz
 if_e	jmp	#L_001_
	cmps	arg2_, #0 wz
 if_ne	mov	result_, arg3_
 if_ne	jmp	#test1_ret
L_001_
	neg	result_, #1
test1_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
result_
	long	0
test1_x_
	long	0
