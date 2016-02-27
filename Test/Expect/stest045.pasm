DAT
	org	0
strlen2
	mov	strlen2_r_, #0
L_001_
	rdbyte	strlen2_tmp001_, strlen2_x_ wz
 if_ne	add	strlen2_x_, #1
 if_ne	add	strlen2_r_, #1
 if_ne	jmp	#L_001_
	mov	result_, strlen2_r_
strlen2_ret
	ret

result_
	long	0
strlen2_r_
	long	0
strlen2_tmp001_
	long	0
strlen2_x_
	long	0
