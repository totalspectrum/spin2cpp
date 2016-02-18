DAT
	org	0
stest025_test1
	cmp	stest025_test1_x_, #0 wc,wz
 if_ne	jmp	#L_002_
	cmp	stest025_test1_y_, #0 wc,wz
 if_ne	jmp	#L_002_
 if_eq	jmp	#L_001_
L_002_
	mov	result_, stest025_test1_z_
	jmp	#stest025_test1_ret
L_001_
	neg	result_, #1
stest025_test1_ret
	ret

result_
	long	0
stest025_test1_x_
	long	0
stest025_test1_y_
	long	0
stest025_test1_z_
	long	0
