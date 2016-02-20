DAT
	org	0
stest024_test1
	cmps	stest024_test1_x_, #0 wz
 if_eq	jmp	#L_001_
	cmps	stest024_test1_y_, #0 wz
 if_ne	mov	result_, stest024_test1_z_
 if_ne	jmp	#stest024_test1_ret
L_001_
	neg	result_, #1
stest024_test1_ret
	ret

result_
	long	0
stest024_test1_x_
	long	0
stest024_test1_y_
	long	0
stest024_test1_z_
	long	0
