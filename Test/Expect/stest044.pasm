DAT
	org	0
strlen
	neg	strlen_r_, #1
L_001_
	rdbyte	strlen_c_, arg1_ wz
	add	arg1_, #1
	add	strlen_r_, #1
 if_ne	jmp	#L_001_
	mov	result_, strlen_r_
strlen_ret
	ret

arg1_
	long	0
result_
	long	0
strlen_c_
	long	0
strlen_r_
	long	0
