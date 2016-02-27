DAT
	org	0
strlen
	neg	strlen_r_, #1
L_001_
	rdbyte	strlen_c_, strlen_x_ wz
	add	strlen_x_, #1
	add	strlen_r_, #1
 if_ne	jmp	#L_001_
	mov	result_, strlen_r_
strlen_ret
	ret

result_
	long	0
strlen_c_
	long	0
strlen_r_
	long	0
strlen_x_
	long	0
