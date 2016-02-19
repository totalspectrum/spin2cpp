DAT
	org	0
stest020_simplemul
	mov	stest020_simplemul_i_, #0
	mov	stest020_simplemul_r_, stest020_simplemul_i_
L_001_
	cmps	stest020_simplemul_i_, stest020_simplemul_y_ wc,wz
 if_ge	jmp	#L_002_
	add	stest020_simplemul_r_, stest020_simplemul_y_
	add	stest020_simplemul_i_, #1
	jmp	#L_001_
L_002_
	mov	result_, stest020_simplemul_r_
stest020_simplemul_ret
	ret

result_
	long	0
stest020_simplemul_i_
	long	0
stest020_simplemul_r_
	long	0
stest020_simplemul_y_
	long	0
