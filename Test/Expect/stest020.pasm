DAT
	org	0
simplemul
	mov	simplemul_i_, #0
	mov	simplemul_r_, #0
L_001_
	cmps	simplemul_i_, simplemul_y_ wc,wz
 if_b	add	simplemul_r_, simplemul_y_
 if_b	add	simplemul_i_, #1
 if_b	jmp	#L_001_
	mov	result_, simplemul_r_
simplemul_ret
	ret

result_
	long	0
simplemul_i_
	long	0
simplemul_r_
	long	0
simplemul_y_
	long	0
