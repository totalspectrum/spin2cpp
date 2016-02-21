DAT
	org	0
test1
	cmps	test1_x_, #0 wz
 if_ne	jmp	#L_002_
	cmps	test1_y_, #0 wz
 if_ne	jmp	#L_002_
 if_eq	jmp	#L_001_
L_002_
	mov	result_, test1_z_
	jmp	#test1_ret
L_001_
	neg	result_, #1
test1_ret
	ret

result_
	long	0
test1_x_
	long	0
test1_y_
	long	0
test1_z_
	long	0
