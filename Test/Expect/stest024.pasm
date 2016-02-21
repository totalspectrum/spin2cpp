DAT
	org	0
test1
	cmps	test1_x_, #0 wz
 if_eq	jmp	#L_001_
	cmps	test1_y_, #0 wz
 if_ne	mov	result_, test1_z_
 if_ne	jmp	#test1_ret
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
